package entity

import (
	"encoding/json"
	"fmt"
	"master/utils/log"
)


type SpaceListRespone struct {
	Code            int            `json:"code"`
	Msg             string         `json:"msg"`
	SpaceList       []Space        `json:"space_list"`
}


func (slr *SpaceListRespone) Init(spaceList *SpaceList) {
	slr.SpaceList = make([]Space, 0)
	for _, space := range spaceList.NameToSpace {
		slr.SpaceList = append(slr.SpaceList, space)
	}
}

func (slr *SpaceListRespone) SerializeToJson() (res []byte, e_str string) {
	seriJson, err := json.Marshal(slr)
	if err != nil {
		e_str := fmt.Sprintf("marshal SpaceListRespone struct to json, error: %v", err)
		log.Error(e_str)
		return nil, e_str
	}
	return seriJson, ""
}

type Status struct {
	Code            int32          `json:"code"`
	Msg             string         `json:"msg"`
}

type PsCreateSpaceRequest struct {
	Space          Space             `json:"space"`
	CurPartition   Partition         `json:"cur_partition"`
}

func (csr *PsCreateSpaceRequest) SerializeToJson() (res []byte, e_str string) {
	seriJson, err := json.Marshal(csr)
	if err != nil {
		e_str := fmt.Sprintf("marshal PsCreateSpaceRequest struct to json, error: %v", err)
		log.Error(e_str)
		return nil, e_str
	}
	return seriJson, ""
}

// router AddSpace api
type AddSpaceRequest struct {
	Space        Space             `json:"space"`
}

func (asr *AddSpaceRequest) SerializeToJson() (res []byte, e_str string) {
	seriJson, err := json.Marshal(asr)
	if err != nil {
		e_str := fmt.Sprintf("marshal AddSpaceRequest struct to json, error: %v", err)
		log.Error(e_str)
		return nil, e_str
	}
	return seriJson, ""
}

type DeleteSpaceRequest struct {
	DbName       string            `json:"db_name"`
	SpaceName    string            `json:"space_name"`
}
type CommonResponse struct {
	Status        Status            `json:"status"`
}

func (dsr *DeleteSpaceRequest) SerializeToJson() (res []byte, e_str string) {
	seriJson, err := json.Marshal(dsr)
	if err != nil {
		e_str := fmt.Sprintf("marshal DeleteSpaceRequest struct to json, error: %v", err)
		log.Error(e_str)
		return nil, e_str
	}
	return seriJson, ""
} 
