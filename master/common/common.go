package common

import (
	"fmt"
	"master/config"
	"strconv"
	"strings"
)

func GetSpaceEtcdKey(dbName, spaceName string) string {
	// etcdKey = "/spaces/mata/{DbName}/{Name}"
	return fmt.Sprintf("%s/%s/%s", config.SpacesMataPrefix, dbName, spaceName)
}

func GetCreatingSpaceEtcdKey(dbName, spaceName string) string {
	// etcdKey = "/creating_spaces/mata/{DbName}/{Name}"
	return fmt.Sprintf("%s/%s/%s", config.CreatingSpacesMataPrefix, dbName, spaceName)
}

func GetDeletedSpaceEtcdKey(dbName, spaceName string, spaceId int) string {
	// etcdKey = "delete_spaces/mata/{DbName}/{Name}/{SpaceId}"
	return fmt.Sprintf("%s/%s/%s/%d", config.DeletedSpacesMataPrefix, dbName, spaceName, spaceId)
}

func GetIPList(keyByteList [][]byte) []string {
	result := make([]string, len(keyByteList))
	for i, b := range keyByteList {
		result[i] = string(b)
	}
	return result
}

func GetPsNodeName(IP string, Port, Id int) string {
	return fmt.Sprintf(":%s:%d:%d", IP, Port, Id)
}

func ParsePsRegisterKey(key string) (IP string, Port, Id int) {
	// key: /nodes/ps/:172.24.131.15:8081:1
	// val: rpc_port
	// TODO: key format check
	parts := strings.Split(key, ":")
	IP = parts[1]
	Id, _ = strconv.Atoi(parts[3])
	Port, _ = strconv.Atoi(parts[2])
	return IP, Port, Id
}

func GetRouterNodeEtcdKey(key string) (IP string, port int) {
	parts := strings.Split(key, ":")
	IP = parts[1]
	port, _ = strconv.Atoi(parts[2])
	return IP, port
}

func GetErrPsNodeEtcdKey(IP string, Port, Id int) string {
	etcdKey := fmt.Sprintf("%s/%s", config.ErrPsNodesPrefix, GetPsNodeName(IP, Port, Id))
	return etcdKey
}

func GetErrSpaceEtcdKey(dbName, spaceName string, spaceId int) string {
	return fmt.Sprintf("%s/%s/%s/%d", config.ErrSpacesPrefix, dbName, spaceName, spaceId)
}

func GetPartitionGroupName(dbName, spaceName string, partitionId int) string {
	return fmt.Sprintf("%s:%s:%d", dbName, spaceName, partitionId)
}
