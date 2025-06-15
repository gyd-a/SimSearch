package clusterapi

import (
	"context"
	"fmt"
	"master/config"
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
	if val, _ := ca.etcdCli.Get(ctx, config.GenPsIdKey); val == nil {
		ca.etcdCli.NewIDGenerate(ctx, config.GenPsIdKey, config.IdBaseVal)
	}

	if val, _ := ca.etcdCli.Get(ctx, config.GenSpaceIdKey); val == nil {
		ca.etcdCli.NewIDGenerate(ctx, config.GenSpaceIdKey, config.IdBaseVal)
	}

	ca.httpSer = gin.Default()
	ca.httpSer.GET("/ping", ca.ping)
	ca.httpSer.POST("/api/gen_id", ca.genID)
	ca.httpSer.POST("/api/create_space", ca.createSpace)
	ca.httpSer.POST("/api/space_list", ca.spaceList)
	ca.httpSer.POST("/api/delete_space", ca.deleteSpace)

	apiPort := fmt.Sprintf(":%d", config.Conf().Masters.Self().ApiPort)
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
		c.JSON(http.StatusOK, gin.H{"error": "params error " + err.Error()})
		return
	}
	ctx := context.Background()
	genId, _ := ca.etcdCli.NewIDGenerate(ctx, config.GenPsIdKey, config.IdBaseVal)
	fmt.Printf("gen_id: %d\n", genId)
	log.Info("-------------gen_id:[%d] success-------------------", genId)
	c.JSON(http.StatusOK, gin.H{"id": genId, "code": 0, "msg": "success"})
}
