package common

import (
	"fmt"
	"master/config"
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
