package clusterapi

import (
	"context"
	"fmt"
	"master/config"
	"master/entity"
	"master/utils/concurrent"
	"master/utils/httputil"
	"master/utils/log"
	"master/utils/timeutil"
	"net/http"

	// "strconv"

	"github.com/gin-gonic/gin"
)

func GetSpaceEtcdPrefix(db_name string, space_name string) string {
	// etcdKey = "/spaces/mata/{DbName}/{Name}"
	var etcdKey string = fmt.Sprintf("%s/%s/%s", config.SpaceMataPrefix,
		db_name, space_name)
	return etcdKey
}

func GetIPList(keyByteList [][]byte) []string {
	result := make([]string, len(keyByteList))
	for i, b := range keyByteList {
		result[i] = string(b)
	}
	return result
}

func ValidateNameAndGetIdlePsNode(ctx context.Context, space *entity.Space, etcdCli *EtcdCli) (idlePsLiss []entity.Replica, e_str string) {
	ps_keys, ps_vals, _ := etcdCli.PrefixScan(ctx, config.PsNodePrefix)
	replicaList := entity.ReplicaList{}
	replicaList.DeserializeFromByte(ps_keys, ps_vals)
	_, spaces_detail, _ := etcdCli.PrefixScan(ctx, config.SpaceMataPrefix)
	busySpaceList := entity.SpaceList{}
	busySpaceList.DeserializeFromByteList(spaces_detail)
	// TODO: 数据校验
	var db_and_space_name string = space.DbName + ":" + space.Name
	if _, ok := busySpaceList.NameToSpace[db_and_space_name]; ok {
		e_str = fmt.Sprintf("DB_name + space_name[%s] already exists, Please reset DB_name and Space_name.",
			db_and_space_name)
		log.Error(e_str)
		return idlePsLiss, e_str
	}
	busyPsKeyMap := busySpaceList.GetAllPsKeyMap()
	idlePsLiss = replicaList.GetAllIdlePsKeys(busyPsKeyMap)
	return idlePsLiss, e_str
}

func ParallelQueryPsCreateSpace(space *entity.Space, operation string) (e_str string) {
	// 定义任务参数类型
	type taskArgs struct {
		partition *entity.Partition
	}
	// 收集所有任务
	var tasks []taskArgs
	for _, partition := range space.Partitions {
		tasks = append(tasks, taskArgs{
			partition: &partition,
		})
	}
	// 定义并发执行函数
	worker := func(args taskArgs) (bool, error) {
		for _, replica := range (*args.partition).Replicas {
			createSpaceReq := entity.CreateSpaceRequest{
				CurPartition: *args.partition,
				Space:        *space,
			}
			log.Info("query Ps_id:%d create space.", replica.PsID)
			req_json, _ := createSpaceReq.SerializeToJson()
			url := fmt.Sprintf("http://%s:%d/PsService/CreateSpace", replica.PsIP, replica.PsPort)
			resp := entity.CreateSpaceResponse{}
			resp_str, err := httputil.PostRequest(url, nil, req_json, &resp)
			fmt.Printf("url:%s, req_json:%s, resp_json:%s\n", url, req_json, resp_str)
			if err != nil || resp.Status.Code != 0 {
				log.Errorf("query Ps_id:%d create space, error:%v, msg:%s, resp_str:%s",
					replica.PsID, err, resp.Status.Msg, resp_str)
				return false, fmt.Errorf("PS %d create space failed: %v", replica.PsID, err)
				// TODO: 有失败是删除之前创建的space
			}
		}
		return true, nil
	}
	// 并发执行，最大并发数设为10（可根据实际情况调整）
	_, errs := concurrent.ParallelExecuteWithResults(worker, tasks, 10)
	// 检查错误
	for _, err := range errs {
		if err != nil {
			return fmt.Sprintf("parallel operation failed: %v", err)
		}
	}
	return ""
}

func CreateSpaceImpl(ctx context.Context, etcdCli *EtcdCli, space *entity.Space,
	idlePsList *[]entity.Replica) (e_str string) {
	timeStr, _, _ := timeutil.GetCurTime()
	space.CreateTime = timeStr
	space.UpdateTime = timeStr
	if len(*idlePsList) < int(space.PartitionNum*space.ReplicaNum) {
		e_str = fmt.Sprintf("not enough idle PS nodes, need %d, but only %d",
			space.PartitionNum*space.ReplicaNum, len(*idlePsList))
		log.Error(e_str)
		return e_str
	}

	var node_idx int = 0
	for pid := 0; pid < int(space.PartitionNum); pid++ {
		partition := entity.Partition{
			PartitionId: pid,
			GroupName:   fmt.Sprintf("%s:%s:%d", space.DbName, space.Name, pid),
			Replicas:    make([]entity.Replica, space.ReplicaNum),
			CreateTime:  timeStr,
			UpdateTime:  timeStr,
		}
		for rid := 0; rid < int(space.ReplicaNum); rid++ {
			partition.Replicas[rid] = (*idlePsList)[node_idx]
			node_idx += 1
		}
		space.Partitions = append(space.Partitions, partition)
	}
	spaceJson, _ := space.SerializeToJson()
	if e_str = ParallelQueryPsCreateSpace(space, "create"); len(e_str) > 0 {
		return e_str
	}
	etcdKey := GetSpaceEtcdPrefix(space.DbName, space.Name)
	if e := etcdCli.Put(ctx, etcdKey, spaceJson); e != nil {
		e_str = "Space Mata info put etcd server error, " + e.Error()
		log.Error(e_str)
		if e_str = ParallelQueryPsCreateSpace(space, "delete"); len(e_str) > 0 {
			return e_str
		}
		return e_str
	}
	log.Info("db_name:%s, space_name:%s, Create space successful !!!", space.DbName, space.Name)
	return ""
}

func QueryRouterAddSpaceApi(ctx context.Context, etcdCli *EtcdCli, space *entity.Space) (e_str string) {
	router_keys, router_vals, _ := etcdCli.PrefixScan(ctx, config.RouterNodePrefix)
	routerReplicaList := entity.RouterReplicaList{}
	routerReplicaList.DeserializeFromByte(router_keys, router_vals)
	for _, router := range routerReplicaList.KeyToPsNode {
		addSpaceReq := entity.AddSpaceRequest{Space: *space}
		addSpaceReq.Space = *space
		log.Info("query router_ip:%s add space.", router.RouterIP)
		req_json, _ := addSpaceReq.SerializeToJson()
		url := fmt.Sprintf("http://%s:%d/space/add_space", router.RouterIP, router.RouterPort)
		resp := entity.AddSpaceResponse{}
		fmt.Printf("url:%s, req_json:%s\n", url, req_json)
		resp_str, err := httputil.PostRequest(url, nil, req_json, resp)

		if err != nil || resp.Status.Code != 0 {
			log.Errorf("query url:%s add space, error:%v, msg:%s, resp_str:%s",
				url, err, resp.Status.Msg, resp_str)
		}
	}
	return ""
}

func (ca *ClusterAPI) createSpace(c *gin.Context) {
	log.Info("-------------create_space api-------------------")
	var space entity.Space
	// 尝试将请求中的 JSON 绑定到 space 结构体
	if err := c.ShouldBindJSON(&space); err != nil {
		log.Errorf("create_space api request params error: %v", err)
		c.JSON(http.StatusOK, gin.H{"code": 10, "msg": "params error " + err.Error()})
		return
	}
	if unvalid_str := space.Validate(); len(unvalid_str) > 0 {
		c.JSON(http.StatusOK, gin.H{"code": 11, "msg": unvalid_str})
		return
	}
	ctx := context.Background()
	idlePsNode, e_str := ValidateNameAndGetIdlePsNode(ctx, &space, ca.etcdCli)
	if len(e_str) > 0 {
		c.JSON(http.StatusOK, gin.H{"code": 12, "msg": e_str})
		return
	}
	if e_str = CreateSpaceImpl(ctx, ca.etcdCli, &space, &idlePsNode); len(e_str) > 0 {
		c.JSON(http.StatusOK, gin.H{"code": 13, "msg": e_str})
		return
	}
	QueryRouterAddSpaceApi(ctx, ca.etcdCli, &space)
	c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "success"})
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
	_, spaces_detail, _ := ca.etcdCli.PrefixScan(ctx, config.SpaceMataPrefix)
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

type delTaskArgs struct {
	nodeType string
	ip       string
	port     int
	reqJson  []byte
}

func deleteOneSpaceImpl(args delTaskArgs) (string, error) {
	var delUrl string
	if args.nodeType == "ps" {
		delUrl = fmt.Sprintf("http://%s:%d/PsService/DeleteSpace", args.ip, args.port)
	} else if args.nodeType == "router" {
		delUrl = fmt.Sprintf("http://%s:%d/space/delete_space", args.ip, args.port)
	}
	resp := entity.DeleteSpaceResponse{}
	resp_str, err := httputil.PostRequest(delUrl, nil, args.reqJson, &resp)
	if err != nil || resp.Status.Code != 0 {
		log.Errorf("delete %s space, query Url:%d, error:%v, msg:%s, resp_str:%s",
			args.nodeType, delUrl, err, resp.Status.Msg, resp_str)
		return resp_str, nil
	}
	log.Info("Delete %s Space url:%s, success delete", args.nodeType, delUrl)
	return "", nil
}

func (ca *ClusterAPI) deleteSpace(c *gin.Context) {
	log.Info("-------------create_space api-------------------")
	var req entity.DeleteSpaceRequest
	// 尝试将请求中的 JSON 绑定到 space 结构体
	if err := c.ShouldBindJSON(&req); err != nil {
		log.Errorf("delete_space api request params error: %v", err)
		c.JSON(http.StatusOK, gin.H{"code": 101, "msg": "params error " + err.Error()})
		return
	}
	req_json, err_str := req.SerializeToJson()
	if err_str != "" {
		log.Errorf("delete_space api request to json error: %v", err_str)
		c.JSON(http.StatusOK, gin.H{"code": 102, "msg": "params error " + err_str})
		return
	}
	ctx := context.Background()
	etcdKey := GetSpaceEtcdPrefix(req.DbName, req.SpaceName)
	keys, spaces_detail, _ := ca.etcdCli.PrefixScan(ctx, etcdKey)
	if len(keys) == 1 && len(spaces_detail) == 1 {
		space_key := string(keys[0])
		if err := ca.etcdCli.Delete(ctx, space_key); err != nil {
			log.Error("delete space_name: %s failed, err:%v", req.SpaceName, err)
			c.JSON(http.StatusOK, gin.H{"code": 103, "msg": "delete faile."})
			return
		}
		busySpaceList := entity.SpaceList{}
		busySpaceList.DeserializeFromByteList(spaces_detail)

		var tasks []delTaskArgs
		for _, space_info := range busySpaceList.NameToSpace {
			for _, partition_info := range space_info.Partitions {
				for _, rep_info := range partition_info.Replicas {
					tasks = append(tasks, delTaskArgs{
						nodeType: "ps",
						ip:       rep_info.PsIP,
						port:     rep_info.PsPort,
						reqJson:  req_json,
					})
				}
			}
		}
		concurrent.ParallelExecuteWithResults(deleteOneSpaceImpl, tasks, 10)
		// TODO: 处理删除失败的ps

		router_keys, router_vals, _ := ca.etcdCli.PrefixScan(ctx, config.RouterNodePrefix)
		routerReplicaList := entity.RouterReplicaList{}
		routerReplicaList.DeserializeFromByte(router_keys, router_vals)
		var routerTasks []delTaskArgs
		for _, router := range routerReplicaList.KeyToPsNode {
			routerTasks = append(routerTasks, delTaskArgs{
				nodeType: "router",
				ip:       router.RouterIP,
				port:     router.RouterPort,
				reqJson:  req_json,
			})
		}
		concurrent.ParallelExecuteWithResults(deleteOneSpaceImpl, routerTasks, 10)
	}
	if err := ca.etcdCli.Delete(ctx, etcdKey); err != nil {
		log.Error("for delete space, etcd delete key:%s failed", etcdKey)
	}
	c.JSON(http.StatusOK, gin.H{"code": 0, "msg": "success"})
}
