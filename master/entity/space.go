package entity

import (
	"encoding/json"
	"fmt"
	"master/utils/log"
	"regexp"
	"strconv"
	"unicode"
)

// "encoding/json"

// space/[dbId]/[spaceId]:[spaceBody]
type Space struct {
	DbName            string            `json:"db_name"`
	Name              string            `json:"name"` //user setting
	PartitionNum      uint              `json:"partition_num"`
	ReplicaNum        uint              `json:"replica_num"`
	Fields            []Field           `json:"fields"`
	Partitions        []Partition       `json:"partitions,omitempty"` // partitionids not sorted
	CreateTime        string            `json:"create_time,omitempty"`           // 2022-12-12 12:12:12+毫秒时间戳
	UpdateTime        string            `json:"update_time,omitempty"` // 2022-12-12 12:12:12+毫秒时间戳
}


type Partition struct {
	PartitionId       int               `json:"partition_id"`
	GroupName         string            `json:"group_name"`
	Replicas          []Replica         `json:"replicas"`    // leader in replicas
	CreateTime        string            `json:"create_time,omitempty"`           // 2022-12-12 12:12:12+毫秒时间戳
	UpdateTime        string            `json:"update_time,omitempty"` // 2022-12-12 12:12:12+毫秒时间戳
}

type Field struct {
	Name              string            `json:"name"`
	Type              string            `json:"type"`
	Dimension         uint              `json:"dimension,omitempty"`
	Index             *Index            `json:"index,omitempty"`
}

type Index struct {
	Type              string           `json:"type"`
	Params            string           `json:"params,omitempty"`
}


func (s *Space) GetAllNodeKeys() (nodeKeys []string) {
	nodeKeys = make([]string, 0)
	for _, partition := range s.Partitions {
		for _, replica := range partition.Replicas {
			nodeKey := replica.PsIP + "-" + strconv.Itoa(replica.PsID)
			nodeKeys = append(nodeKeys, nodeKey)
		}
	}
	return nodeKeys
}

func (s *Space) SerializeToJson() ([]byte, error) {
	jsonBytes, err := json.Marshal(s)
	if err != nil {
		log.Error("marshal Space struct to json, error: %v", err)
		return nil, err
	}
	return jsonBytes, err
}

func (s *Space) Validate() (string) {
	if err_str := NameValidate(s.DbName); err_str != "" {
		err_str = "DbName 格式错误：" + err_str
		log.Error(err_str)
		return err_str
	}
	if err_str := NameValidate(s.Name); err_str != "" {
		err_str = "Space Name 格式错误：" + err_str
		log.Error(err_str)
		return err_str
	}
	if s.PartitionNum <= 0 || s.PartitionNum > 100 {
		err_str := fmt.Sprintf("PartitionNum[%d] error, num range [1, 100]", s.PartitionNum)
		log.Error(err_str)
		return err_str
	}
	if s.ReplicaNum <= 0 || s.ReplicaNum % 2 == 0 || s.ReplicaNum > 5 {
		err_str := fmt.Sprintf("ReplicaNum[%d] error, it should in (1, 3, 5)", s.ReplicaNum)
		log.Error(err_str)
		return err_str
	}
	if len(s.Partitions) != 0 {
		err_str := "CreateSpace api, Partitions info should is null"
		log.Error(err_str)
		return err_str
	}
	// TODO: Field字段校验
	fmt.Print("Space data Validate pass\n")
	return ""
}

func NameValidate(s string) (string) {
	// 条件 1：长度 > 5
	if len(s) <= 5 {
		return "长度应该 > 5"
	}
	// 条件 2：首字符是字母
	first := rune(s[0])
	if !unicode.IsLetter(first) {
		return "首字母应该为大/小写字母"
	}
	// 条件 3：只包含字母、数字、- 和 _
	if matched, _ := regexp.MatchString(`^[a-zA-Z0-9_-]+$`, s); matched == false {
		return "应该只包含大小字母、数字、'-' 或者 '_'"
	}
	return ""
}

type SpaceList struct {
	NameToSpace          map[string]Space
}

func (sl *SpaceList) DeserializeFromByteList(spaceJsonList [][]byte) {
	sl.NameToSpace = make(map[string]Space)
	for _, spaceJson := range spaceJsonList {
		var s Space
		if err := json.Unmarshal(spaceJson, &s); err != nil {
			log.Error("unmarshal json of ETCD space to Space struct, json:%s error: %v", spaceJson, err)
			continue
		}
		sl.NameToSpace[s.DbName + ":" + s.Name] = s
	}
}

func (sl *SpaceList) SerializeToJsonList() (nameToSpaceJson map[string]string) {
	nameToSpaceJson = make(map[string]string)
	for db_and_space_name, space := range sl.NameToSpace {
		spaceJson, err := json.Marshal(space)
		if err != nil {
			log.Error("marshal space of ETCD struct to json, db_and_space_name:%s error: %v",
					  db_and_space_name, err)
			continue
		}
		nameToSpaceJson[db_and_space_name] = string(spaceJson)
	}
	return nameToSpaceJson
}

func (sl *SpaceList) GetAllPsKeyMap() (PsKeyMap map[string]string) {
	PsKeyMap = make(map[string]string)
	for _, space := range sl.NameToSpace {
		for _, partition := range space.Partitions {
			for _, replica := range partition.Replicas {
				nodeKey := replica.PsIP + "-" + strconv.Itoa(replica.PsID)
				PsKeyMap[nodeKey] = ""
			}
		}
	}
	return PsKeyMap
}