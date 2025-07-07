package clusterapi

import (
	"context"
	"fmt"
	"master/common"
	"master/config"
	cfg "master/config"
	"master/entity"
	"master/etcdcluster"
	"master/utils/log"
	"net/http"

	// "strconv"

	"github.com/gin-gonic/gin"
)

type EtcdCli = etcdcluster.EtcdClient

type ClusterAPI struct {
	etcdCli *EtcdCli
	httpSer *gin.Engine
}

func (ca *ClusterAPI) InitClusterApi(etcdClient *EtcdCli) {
	ca.etcdCli = etcdClient
	ctx := context.Background()
	if val, _ := ca.etcdCli.Get(ctx, cfg.GenPsIdKey); val == nil {
		ca.etcdCli.NewIDGenerate(ctx, cfg.GenPsIdKey, cfg.IdBaseVal)
	}

	if val, _ := ca.etcdCli.Get(ctx, cfg.GenSpaceIdKey); val == nil {
		ca.etcdCli.NewIDGenerate(ctx, cfg.GenSpaceIdKey, cfg.IdBaseVal)
	}

	ca.httpSer = gin.Default()
	ca.httpSer.GET("/ping", ca.ping)
	ca.httpSer.POST("/api/gen_id", ca.genID)
	ca.httpSer.POST("/api/register", ca.register)
	ca.httpSer.POST("/api/create_space", ca.createSpace)
	ca.httpSer.POST("/api/space_list", ca.spaceList)
	ca.httpSer.POST("/api/delete_space", ca.deleteSpace)

	apiPort := fmt.Sprintf(":%d", cfg.Conf().Masters.Self().ApiPort)
	go func() {
		ca.httpSer.Run(apiPort)
		fmt.Print("------------ClusterAPI stop---------")
	}()
}

func (ca *ClusterAPI) ping(c *gin.Context) {
	log.Info("-------------ping api-------------------")
	c.JSON(http.StatusOK, gin.H{
		"message": "ping",
	})
}

// got every partition servers system info
func (ca *ClusterAPI) genID(c *gin.Context) {
	var req struct {
		Id int `json:"id"`
	}
	log.Info("-------------gen_id api-------------------")
	if err := c.ShouldBindJSON(&req); err != nil {
		log.Errorf("gen_id api request params error: %v", err)
		c.JSON(http.StatusOK, gin.H{"code": 10, "msg": "params error " + err.Error(), "id": -1})
		return
	}
	ctx := context.Background()
	genId, _ := ca.etcdCli.NewIDGenerate(ctx, cfg.GenPsIdKey, cfg.IdBaseVal)
	fmt.Printf("gen_id: %d\n", genId)
	log.Info("-------------gen_id:[%d] success-------------------", genId)
	c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "success", "id": genId})
}

func (ca *ClusterAPI) register(c *gin.Context) {
	var req struct {
		Ip   string `json:"ip"`
		Port int    `json:"port"`
		Id   int    `json:"id"`
	}
	log.Info("-------------gen_id api-------------------")
	err := c.ShouldBindJSON(&req)
	if err != nil {
		log.Errorf("register api request params error: %v", err)
		c.JSON(http.StatusOK, gin.H{"code": 10, "msg": "params error " + err.Error(), "register_key": ""})
		return
	}
	ctx := context.Background()
	nodePrefix := fmt.Sprintf("%s/%s", config.PsNodesPrefix, common.GetPsNodeName(req.Ip, req.Port))
	errNodePrefix := fmt.Sprintf("%s/%s", config.ErrPsNodesPrefix, common.GetPsNodeName(req.Ip, req.Port))
	code, msg_str, nodeKey, ddlLocker := 0, "success", "", (*etcdcluster.EtcdLocker)(nil)
	defer func() {
		if ddlLocker != nil {
			ddlLocker.UnlockAndClose(ctx)
		}
		c.JSON(http.StatusOK, gin.H{"code": code, "msg": msg_str, "register_key": nodeKey})
	}()
	if ddlLocker, err = etcdcluster.GetEtcdLocker(ca.etcdCli.GetCli(), cfg.DdlLockKey); err != nil {
		code, msg_str = 10, fmt.Sprintf("get etcd locker failed: %v", err)
		log.Error(msg_str)
		return
	}
	if err = ddlLocker.TryLock(ctx, 20, 200); err != nil {
		code, msg_str = 11, fmt.Sprintf("TryLock etcd locker failed: %v", err)
		log.Error(msg_str)
		return
	}
	psKeys, psVals, _ := ca.etcdCli.PrefixScan(ctx, nodePrefix)
	errPsKeys, errPsVals, _ := ca.etcdCli.PrefixScan(ctx, errNodePrefix)
	var replicaList, errPeplicaList entity.ReplicaList
	replicaList.RepDeserializeFromByte(psKeys, psVals)
	errPeplicaList.RepDeserializeFromByte(errPsKeys, errPsVals)

	_, spaces_detail, _ := ca.etcdCli.PrefixScan(ctx, cfg.SpacesMataPrefix)
	busySpaceList := entity.SpaceList{}
	busySpaceList.DeserializeFromByteList(spaces_detail)
	busyPsKeyMap := busySpaceList.GetAllPsMap()

	curNodeName := common.GetPsNodeName(req.Ip, req.Port)
	nodeMapList := []map[string]entity.Replica{busyPsKeyMap, replicaList.KeyToPsNode, errPeplicaList.KeyToPsNode}
	for _, nodeMap := range nodeMapList {
		if nodeInfo, ok := nodeMap[curNodeName]; ok && nodeInfo.PsID != req.Id {
			msg_str = fmt.Sprintf("register node ip:%s port:%d already exists. old_id:%d new_id:%d", req.Ip, req.Port, nodeInfo.PsID, req.Id)
			code = 12
			log.Error(msg_str)
			return
		}
	}
	nodeKey = common.GetPsRegisterKey(req.Ip, req.Port, req.Id)
	ca.etcdCli.CreateWithTTL(ctx, nodeKey, []byte("by_master_put"), cfg.PsNodeTTL)
	log.Info("-------------register ps node success, register_key:%s-------------------", nodeKey)
}
