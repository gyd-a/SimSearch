package entity

import (
	"master/utils/log"
	"strconv"
	"strings"
)


type Replica struct {
	PsID              int               `json:"ps_id"`
	PsIP              string            `json:"ps_ip"`
	PsPort            int               `json:"ps_port"`
	IsLeader          int               `json:"is_leader"`   // 0: 不是，1: 是
	NodeStatus        string            `json:"node_status"`
	Msg               string            `json:"msg"`
}

type ReplicaList struct {
	KeyToPsNode         map[string]Replica
}

func (r *ReplicaList)DeserializeFromByte(nodeKeys [][]byte, vals [][]byte) {
	r.KeyToPsNode = make(map[string]Replica)
	// key: /nodes/ps/:1:172.24.131.15:8081
	// val: rpc_port
	for _, nodeKey := range nodeKeys {
		strNodeKey := string(nodeKey)
		// strVal := string(vals[idx])
		if _, ok := r.KeyToPsNode[strNodeKey]; ok {
			log.Error("duplicate ps nodeKey: %s", strNodeKey)
			continue
		}
		// TODO: check nodeKey format
		parts := strings.Split(strNodeKey, ":")
		psIP := parts[2]
		psId, _ := strconv.Atoi(parts[1])
		psPort, _ := strconv.Atoi(parts[3])
		r.KeyToPsNode[strNodeKey] = Replica{PsID: psId, PsIP: psIP, PsPort: psPort,
											IsLeader:0}
	}
}

func (r *ReplicaList) GetAllIdlePsKeys(busyNodeKeys map[string]string) (idleNodes []Replica) {
	idleNodes = make([]Replica, 0)
	for key, node := range r.KeyToPsNode {
		if _, ok := busyNodeKeys[key]; !ok {
			idleNodes = append(idleNodes, node)
		}
	}
	return idleNodes
}


type RouterReplica struct {
	RouterIP       string            `json:"router_ip"`
	RouterPort     int 			     `json:"router_port"`
}

type RouterReplicaList struct {
	KeyToPsNode         map[string]RouterReplica
}

func (r *RouterReplicaList)DeserializeFromByte(nodeKeys [][]byte, vals [][]byte) {
	r.KeyToPsNode = make(map[string]RouterReplica)
	// key: /nodes/router/:172.24.131.15:8081
	// val: 暂无信息
	for _, nodeKey := range nodeKeys {
		strNodeKey := string(nodeKey)
		// strVal := string(vals[idx])
		if _, ok := r.KeyToPsNode[strNodeKey]; ok {
			log.Error("duplicate router nodeKey: %s", strNodeKey)
			continue
		}
		// TODO: check nodeKey format
		parts := strings.Split(strNodeKey, ":")
		routerIP := parts[1]
		routerPort, _ := strconv.Atoi(parts[2])
		r.KeyToPsNode[strNodeKey] = RouterReplica{RouterIP: routerIP, RouterPort: routerPort}
	}
}
