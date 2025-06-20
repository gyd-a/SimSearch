package clusterapi

import (
	"context"
	"fmt"
	cfg "master/config"
	"master/entity"
	"master/etcdcluster"
	"master/monitor"
	"master/utils/concurrent"
	"master/utils/httputil"
	"master/utils/log"
	"master/utils/timeutil"
	"net/http"

	"master/common"
	// "strconv"

	"github.com/gin-gonic/gin"
)

func ValidateNameAndGetIdlePsNode(ctx context.Context, space *entity.Space, etcdCli *EtcdCli) (idlePsList []entity.Replica, e_str string) {
	psKeys, psVals, _ := etcdCli.PrefixScan(ctx, cfg.PsNodesPrefix)
	errPsKeys, errPsVals, _ := etcdCli.PrefixScan(ctx, cfg.ErrPsNodesPrefix)

	var replicaList, errPeplicaList entity.ReplicaList
	replicaList.RepDeserializeFromByte(psKeys, psVals)
	errPeplicaList.RepDeserializeFromByte(errPsKeys, errPsVals)

	_, spaces_detail, _ := etcdCli.PrefixScan(ctx, cfg.SpacesMataPrefix)
	busySpaceList := entity.SpaceList{}
	busySpaceList.DeserializeFromByteList(spaces_detail)
	// TODO: 数据校验
	var db_and_space_name string = space.DbName + ":" + space.SpaceName
	if _, ok := busySpaceList.NameToSpace[db_and_space_name]; ok {
		e_str = fmt.Sprintf("DB_name + space_name[%s] already exists, Please reset DB_name and Space_name.",
			db_and_space_name)
		log.Error(e_str)
		return idlePsList, e_str
	}
	busyPsKeyMap := busySpaceList.GetAllPsMap()
	unusableNodeMaps := []map[string]entity.Replica{busyPsKeyMap, errPeplicaList.KeyToPsNode}
	idlePsList = replicaList.GetAllIdlePsKeys(unusableNodeMaps)
	return idlePsList, e_str
}

type taskArgs struct {
	nodeType string
	ip       string
	port     int
	reqJson  []byte
}

func HttpOneNode(args taskArgs) (resp entity.CommonResponse, err_str string) {
	var url string
	if args.nodeType == "ps_delete" {
		url = fmt.Sprintf("http://%s:%d/PsService/DeleteSpace", args.ip, args.port)
	} else if args.nodeType == "router_delete" {
		url = fmt.Sprintf("http://%s:%d/space/delete_space", args.ip, args.port)
	} else if args.nodeType == "ps_create" {
		url = fmt.Sprintf("http://%s:%d/PsService/CreateSpace", args.ip, args.port)
	} else if args.nodeType == "router_create" {
		url = fmt.Sprintf("http://%s:%d/space/add_space", args.ip, args.port)
	}
	resp_str, err_str := httputil.PostRequest(url, nil, args.reqJson, &resp)
	if err_str != "" || resp.Status.Code != 0 {
		log.Errorf("%s space, query Url:%d, error:%v, msg:%s, resp_str:%s",
			args.nodeType, url, err_str, resp.Status.Msg, resp_str)
		resp.Status.Msg = fmt.Sprintf("%s, detail: %s", resp.Status.Msg, err_str)
		return resp, err_str
	}
	log.Info("%s Space url:%s success", args.nodeType, url)
	return resp, err_str
}

func ParallelCreatePsSpace(space *entity.Space) (failedPsIdxs []int) {
	// 收集所有任务
	var tasks []taskArgs
	for _, partition := range space.Partitions {
		req := entity.PsCreateSpaceRequest{Space: *space, CurPartition: partition}
		reqJson, _ := req.SerializeToJson()
		for _, rep := range partition.Replicas {
			tasks = append(tasks, taskArgs{
				nodeType: "ps_create",
				ip:       rep.PsIP,
				port:     rep.PsPort,
				reqJson:  reqJson})
		}
	}
	// 并发执行，最大并发数设为10（可根据实际情况调整）
	respList, errList := concurrent.ParallelExecuteWithResults(HttpOneNode, tasks, 10)
	// 检查错误
	for idx, resp := range respList {
		if errList[idx] != "" || resp.Status.Code != 0 {
			errRep := space.Partitions[idx/space.PartitionNum].Replicas[idx%space.PartitionNum]
			log.Errorf("Ps Create space failed, ip:%s port:%d id:%d, space_name:%s, resp code:%d, msg:%s, err:%s",
				errRep.PsIP, errRep.PsPort, errRep.PsID, space.SpaceName, resp.Status.Code, resp.Status.Msg, errList[idx])
			failedPsIdxs = append(failedPsIdxs, idx)
		}
	}
	return failedPsIdxs
}

func ParallelDeletePsSpace(ctx context.Context, space *entity.Space) (failedPsIdxs []int) {
	req := entity.DeleteSpaceRequest{
		DbName:    space.DbName,
		SpaceName: space.SpaceName,
	}
	req_json, _ := req.SerializeToJson()
	var tasks []taskArgs
	for _, partition_info := range space.Partitions {
		for _, rep_info := range partition_info.Replicas {
			tasks = append(tasks, taskArgs{
				nodeType: "ps_delete",
				ip:       rep_info.PsIP,
				port:     rep_info.PsPort,
				reqJson:  req_json,
			})
		}
	}
	respList, errList := concurrent.ParallelExecuteWithResults(HttpOneNode, tasks, 10)
	for idx, resp := range respList {
		if errList[idx] != "" || resp.Status.Code != 0 {
			errRep := space.Partitions[idx/space.PartitionNum].Replicas[idx%space.PartitionNum]
			log.Errorf("Ps Delete space failed, ip:%s port:%d id:%d, space_name:%s, resp code:%d, msg:%s, err:%s",
				errRep.PsIP, errRep.PsPort, errRep.PsID, space.SpaceName, resp.Status.Code, resp.Status.Msg, errList[idx])
			failedPsIdxs = append(failedPsIdxs, idx)
		}
	}
	return failedPsIdxs
}

func DeleteRouterSpace(ctx context.Context, etcdCli *EtcdCli, dbName, spaceName string) {
	req := entity.DeleteSpaceRequest{
		DbName:    dbName,
		SpaceName: spaceName,
	}
	req_json, _ := req.SerializeToJson()
	if router_keys, router_vals, err := etcdCli.PrefixScan(ctx, cfg.RouterNodesPrefix); err == nil {
		routerReplicaList := entity.RouterReplicaList{}
		routerReplicaList.DeserializeFromByte(router_keys, router_vals)
		var routerTasks []taskArgs
		for _, router := range routerReplicaList.KeyToPsNode {
			routerTasks = append(routerTasks, taskArgs{
				nodeType: "router_delete",
				ip:       router.RouterIP,
				port:     router.RouterPort,
				reqJson:  req_json,
			})
		}
		concurrent.ParallelExecuteWithResults(HttpOneNode, routerTasks, 10)
	} else {
		log.Error("DeleteRouterSpace(), scan router nodes failed, err: %v", err)
		return
	}
}

func AddRouterSpace(ctx context.Context, etcdCli *EtcdCli, space *entity.Space) (e_str string) {
	router_keys, router_vals, _ := etcdCli.PrefixScan(ctx, cfg.RouterNodesPrefix)
	routerReplicaList := entity.RouterReplicaList{}
	routerReplicaList.DeserializeFromByte(router_keys, router_vals)
	addSpaceReq := entity.AddSpaceRequest{Space: *space}
	var tasks []taskArgs
	req_json, _ := addSpaceReq.SerializeToJson()
	for _, router := range routerReplicaList.KeyToPsNode {
		tasks = append(tasks, taskArgs{
			nodeType: "router_create",
			ip:       router.RouterIP,
			port:     router.RouterPort,
			reqJson:  req_json,
		})
	}
	respList, errList := concurrent.ParallelExecuteWithResults(HttpOneNode, tasks, 10)
	errCnt := 0
	for idx, resp := range respList {
		task := tasks[idx]
		if errList[idx] != "" || resp.Status.Code != 0 {
			errCnt += 1
			log.Error("AddRouterSpace(), router node ip:%s, port:%d, resp code:%d, msg:%s add failed",
				task.ip, task.port, resp.Status.Code, resp.Status.Msg)
		}
	}
	log.Infof("AddRouterSpace(), add space to router nodes, %d/%d", errCnt, len(respList))
	return ""
}

func PutNodesErrInfoToEtcd(ctx context.Context, etcdCli *EtcdCli, space *entity.Space,
						   failedIdxs []int, errMsg string) (failedNodesStr string) {
	if len(failedIdxs) > 0 {
		for _, idx := range failedIdxs {
			rep := space.Partitions[idx/space.PartitionNum].Replicas[idx%space.PartitionNum]
			errPsNodeKey := common.GetErrPsNodeEtcdKey(rep.PsIP, rep.PsPort, rep.PsID)
			msg := fmt.Sprintf("space_id:%d %s", space.Id, errMsg)
			etcdCli.PutOrAppend(ctx, errPsNodeKey, []byte(msg), cfg.EtcdValMaxLen)
			failedNodesStr += fmt.Sprintf("ip:%s, port:%d, id:%d; ", rep.PsIP, rep.PsPort, rep.PsID)
		}
	}
	return failedNodesStr
}

func CreatePsSpaceImpl(ctx context.Context, etcdCli *EtcdCli, space *entity.Space,
					   idlePsList *[]entity.Replica) (isAbledDdl bool, e_str string) {
	log_prefix := fmt.Sprintf("CreateSpace(db_name:%s, space_name:%s)", space.DbName, space.SpaceName)
	timeStr, _, _ := timeutil.GetCurTime()
	space.CreateTime, space.UpdateTime = timeStr, timeStr
	if len(*idlePsList) < space.PartitionNum*space.ReplicaNum {
		e_str = fmt.Sprintf("%s, not enough idle PS nodes, need %d, but only %d, Create space failed",
			log_prefix, space.PartitionNum*space.ReplicaNum, len(*idlePsList))
		log.Error(e_str)
		return true, e_str
	}

	var node_idx int = 0
	for pid := 0; pid < space.PartitionNum; pid++ {
		groupName := common.GetPartitionGroupName(space.DbName, space.SpaceName, pid)
		partition := entity.CreatePartition(groupName, pid, space.ReplicaNum, timeStr)
		for rid := 0; rid < space.ReplicaNum; rid++ {
			partition.Replicas[rid] = (*idlePsList)[node_idx]
			node_idx += 1
		}
		space.Partitions = append(space.Partitions, partition)
	}
	etcdKey := common.GetSpaceEtcdKey(space.DbName, space.SpaceName)
	spaceId, _ := etcdCli.NewIDGenerate(ctx, cfg.GenSpaceIdKey, cfg.IdBaseVal)
	space.Id = int(spaceId)
	errSpaceKey := common.GetErrSpaceEtcdKey(space.DbName, space.SpaceName, space.Id)

	spaceJson, _ := space.SerializeToJson()
	etcdCli.Put(ctx, cfg.DdlDisabledKey, append(spaceJson, []byte("; ")...))
	if failedPsIdxs := ParallelCreatePsSpace(space); len(failedPsIdxs) > 0 {
		e_str = log_prefix + ", Create ps space failed, deadly error; "
		log.Error(e_str)
		failedNodesStr := PutNodesErrInfoToEtcd(ctx, etcdCli, space, failedPsIdxs, e_str)
		CreateErrInfo := "create ps space failed; failed nodes: " + failedNodesStr + "; "
		etcdCli.PutOrAppend(ctx, errSpaceKey, []byte(CreateErrInfo), cfg.EtcdValMaxLen)
		etcdCli.PutOrAppend(ctx, cfg.DdlDisabledKey, []byte(CreateErrInfo), cfg.EtcdValMaxLen)

		deletedFailedPsIdxs := ParallelDeletePsSpace(ctx, space)
		failedNodesStr = PutNodesErrInfoToEtcd(ctx, etcdCli, space, deletedFailedPsIdxs, "delete ps space failed; ")
		if len(failedNodesStr) > 0 {
			delErrInfo := "delete ps failed nodes: " + failedNodesStr + "; "
			etcdCli.PutOrAppend(ctx, cfg.DdlDisabledKey, []byte(delErrInfo), cfg.EtcdValMaxLen)
		}
		return false, e_str
	}
	if e := etcdCli.Put(ctx, etcdKey, spaceJson); e != nil {
		e_str = fmt.Sprintf("%s, Put space mata info to etcd failed, deadly error:%v; ", log_prefix, e.Error())
		log.Error(e_str)
		etcdCli.PutOrAppend(ctx, errSpaceKey, []byte(e_str), cfg.EtcdValMaxLen)
		deletedFailedPsIdxs := ParallelDeletePsSpace(ctx, space)
		if failedNodesStr := PutNodesErrInfoToEtcd(ctx, etcdCli, space, deletedFailedPsIdxs, e_str); len(failedNodesStr) > 0 {
			etcdCli.PutOrAppend(ctx, cfg.DdlDisabledKey, []byte("delete ps failed nodes: "+failedNodesStr+"; "), cfg.EtcdValMaxLen)
		}
		return false, e_str
	}
	etcdCli.Delete(ctx, cfg.DdlDisabledKey)
	return true, ""
}

func (ca *ClusterAPI) ValidateCreateSpaceParam(c *gin.Context) (space entity.Space, err_str string) {
	// 尝试将请求中的 JSON 绑定到 space 结构体
	if err := c.ShouldBindJSON(&space); err != nil {
		log.Errorf("create_space api request params error: %v", err)
		return space, "params error " + err.Error()
	}
	if unvalid_str := space.Validate(); len(unvalid_str) > 0 {
		return space, unvalid_str
	}
	return space, ""
}

func (ca *ClusterAPI) createSpace(c *gin.Context) {
	log.Info("-------------create_space api-------------------")
	ctx, code, msg_str, ddlLocker := context.Background(), 10, "", (*etcdcluster.EtcdLocker)(nil)
	space, isDdlAbled, idlePsNode, err := entity.Space{}, true, []entity.Replica{}, error(nil)
	defer func() {
		if ddlLocker != nil {
			ddlLocker.UnlockAndClose(ctx)
		}
		c.JSON(http.StatusOK, gin.H{"code": code, "msg": msg_str})
	}()
	space, msg_str = ca.ValidateCreateSpaceParam(c)
	log_prefix := fmt.Sprintf("CreateSpace(db_name:%s, space_name:%s)", space.DbName, space.SpaceName)
	if msg_str != "" {
		return
	}
	ddlLocker, err = etcdcluster.GetEtcdLocker(ca.etcdCli.GetCli(), cfg.DdlLockKey)
	if err != nil {
		msg_str = fmt.Sprintf("%s, get etcd locker failed: %v", log_prefix, err)
		log.Error(msg_str)
		return
	}
	if ddlLocker.TryLock(ctx, 20, 200) != nil {
		msg_str = log_prefix + ", get etcd locker failed"
		return
	}
	if val, e := ca.etcdCli.Get(ctx, cfg.DdlDisabledKey); e != nil || val != nil {
		msg_str = fmt.Sprintf("%s, cluster has error, cannot create space.", log_prefix)
		log.Error(msg_str)
		return
	}
	idlePsNode, msg_str = ValidateNameAndGetIdlePsNode(ctx, &space, ca.etcdCli)
	if len(msg_str) > 0 {
		return
	}
	if isDdlAbled, msg_str = CreatePsSpaceImpl(ctx, ca.etcdCli, &space, &idlePsNode); len(msg_str) > 0 {
		if !isDdlAbled {
			code = 100
		}
	} else {
		AddRouterSpace(ctx, ca.etcdCli, &space)
		msg_str = "success"
		code = 0
		log.Info("%s, Create space successful !!!", log_prefix)
	}
}

func (ca *ClusterAPI) spaceList(c *gin.Context) {
	log.Info("-------------space_list api-------------------")
	var req struct {
		NamePrefix string `json:"name_prefix"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		log.Errorf("space_list api request params error: %v", err)
		c.JSON(http.StatusOK, gin.H{"code": 10, "msg": "params error " + err.Error()})
		return
	}
	ctx := context.Background()
	_, spaces_detail, _ := ca.etcdCli.PrefixScan(ctx, cfg.SpacesMataPrefix)
	spaceList := entity.SpaceList{}
	spaceList.DeserializeFromByteList(spaces_detail)
	resp := entity.SpaceListRespone{Code: 0, Msg: ""}
	resp.Init(&spaceList)
	if resp_byte, e_str := resp.SerializeToJson(); len(e_str) == 0 {
		c.Data(http.StatusOK, "application/json", resp_byte)
	} else {
		c.JSON(http.StatusOK, gin.H{"code": 10, "msg": e_str})
	}
}

func (ca *ClusterAPI) deleteSpace(c *gin.Context) {
	log.Info("-------------create_space api-------------------")
	ctx, code, msg_str, ddlLocker := context.Background(), 100, "", (*etcdcluster.EtcdLocker)(nil)
	req, err := entity.DeleteSpaceRequest{}, error(nil)
	defer func() {
		if ddlLocker != nil {
			ddlLocker.UnlockAndClose(ctx)
		}
		c.JSON(http.StatusOK, gin.H{"code": code, "msg": msg_str})
	}()
	// 尝试将请求中的 JSON 绑定到 space 结构体
	if err = c.ShouldBindJSON(&req); err != nil {
		msg_str, code = "delete_space api request params error: "+err.Error(), 10
		log.Errorf(msg_str)
		return
	}
	log_prefix := fmt.Sprintf("DeleteSpace(db_name:%s, space_name:%s)", req.DbName, req.SpaceName)
	if ddlLocker, err = etcdcluster.GetEtcdLocker(ca.etcdCli.GetCli(), cfg.DdlLockKey); err != nil {
		code = 10
		msg_str = fmt.Sprintf("%s failed, get etcd locker failed: %v", log_prefix, err)
		log.Errorf(msg_str)
		return
	}
	ddlLocker.TryLock(ctx, 20, 200)
	spaceEtcdKey := common.GetSpaceEtcdKey(req.DbName, req.SpaceName)
	if space_detail, e := ca.etcdCli.Get(ctx, spaceEtcdKey); e == nil && space_detail != nil {
		if val, e := ca.etcdCli.Get(ctx, cfg.DdlDisabledKey); e != nil || val != nil {
			msg_str = fmt.Sprintf("%s, cluster has error, cannot create space.", log_prefix)
			log.Error(msg_str)
			return
		}
		space := entity.Space{}
		if err = space.DeserializeFromByte(space_detail); err != nil {
			msg_str = fmt.Sprintf("%s, deserialize space_json failed, err: %v; ", log_prefix, err)
			monitor.AddErrorInfo("deserialize_space_json", 3, msg_str)
			ca.etcdCli.PutOrAppend(ctx, cfg.ErrLog, []byte(msg_str), cfg.ErrLogValMaxLen)
			log.Error(msg_str)
			return
		}
		deletedSpaceKey := common.GetDeletedSpaceEtcdKey(req.DbName, req.SpaceName, space.Id)
		errSpaceKey := common.GetErrSpaceEtcdKey(req.DbName, req.SpaceName, space.Id)
		ca.etcdCli.Put(ctx, deletedSpaceKey, space_detail)
		ca.etcdCli.Put(ctx, cfg.DdlDisabledKey, []byte(log_prefix))
		if err = ca.etcdCli.Delete(ctx, spaceEtcdKey); err != nil {
			monitor.AddErrorInfo("delete etcd space infor error", 3, log_prefix)
			ca.etcdCli.PutOrAppend(ctx, errSpaceKey, []byte("delete etcd space info failed; "), cfg.EtcdValMaxLen)
			log.Error("%s delete etcd space failed, deadly error:%v", log_prefix, err)
			return
		}
		DeleteRouterSpace(ctx, ca.etcdCli, req.DbName, req.SpaceName)
		if failedPsIdxs := ParallelDeletePsSpace(ctx, &space); len(failedPsIdxs) > 0 {
			failedNodesStr := PutNodesErrInfoToEtcd(ctx, ca.etcdCli, &space, failedPsIdxs, log_prefix+" delete ps space failed; ")
			ca.etcdCli.PutOrAppend(ctx, errSpaceKey, []byte("delete ps space failed; failed nodes: "+failedNodesStr), cfg.EtcdValMaxLen)
			ca.etcdCli.Delete(ctx, cfg.DdlDisabledKey)
			msg_str = fmt.Sprintf("%s delete ps space failed, deadly error nodes:%s", log_prefix, failedNodesStr)
			log.Error(msg_str)
			return
		}
		ca.etcdCli.Delete(ctx, cfg.DdlDisabledKey)
		ca.etcdCli.PutOrAppend(ctx, deletedSpaceKey, []byte("; success"), cfg.EtcdValMaxLen)
		code, msg_str = 0, "success"
	} else {
		code, msg_str = 10, fmt.Sprintf("%s is not exist", log_prefix)
		log.Error("%s, delete space failed, err: %v", log_prefix, e)
	}
}
