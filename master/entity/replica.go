package entity

import (
	"master/common"
	"master/utils/log"
)

type Replica struct {
	PsID       int    `json:"ps_id"`
	PsIP       string `json:"ps_ip"`
	PsPort     int    `json:"ps_port"`
	IsLeader   int    `json:"is_leader"` // 0: 不是，1: 是
	NodeStatus string `json:"node_status"`
	Msg        string `json:"msg"`
}

type ReplicaList struct {
	KeyToPsNode map[string]Replica
}

func (r *ReplicaList) RepDeserializeFromByte(nodeKeys [][]byte, vals [][]byte) {
	r.KeyToPsNode = make(map[string]Replica)
	for _, nodeKey := range nodeKeys {
		psIP, psPort, psId := common.ParsePsRegisterKey(string(nodeKey))
		strNodeKey := common.GetPsNodeName(psIP, psPort, psId)
		// strVal := string(vals[idx])
		if _, ok := r.KeyToPsNode[strNodeKey]; ok {
			log.Error("duplicate ps nodeKey: %s", strNodeKey)
			continue
		}
		r.KeyToPsNode[strNodeKey] = Replica{PsID: psId, PsIP: psIP, PsPort: psPort,
			IsLeader: 0}
	}
}

func (r *ReplicaList) GetAllIdlePsKeys(busyNodeKeysList []map[string]Replica) (idleNodes []Replica) {
	for key, node := range r.KeyToPsNode {
		ok := false
		for _, busyNodeKeys := range busyNodeKeysList {
			if _, ok = busyNodeKeys[key]; ok {
				break
			}
		}
		if !ok {
			idleNodes = append(idleNodes, node)
		}
	}
	return idleNodes
}

type RouterReplica struct {
	RouterIP   string `json:"router_ip"`
	RouterPort int    `json:"router_port"`
}

type RouterReplicaList struct {
	KeyToPsNode map[string]RouterReplica
}

func (r *RouterReplicaList) DeserializeFromByte(nodeKeys [][]byte, vals [][]byte) {
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
		routerIP, routerPort := common.GetRouterNodeEtcdKey(strNodeKey)
		r.KeyToPsNode[strNodeKey] = RouterReplica{RouterIP: routerIP, RouterPort: routerPort}
	}
}
