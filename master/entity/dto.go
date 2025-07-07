package entity

import (
	"encoding/json"
	"fmt"
	cfg "master/config"
	"master/utils/genericutil"
	"master/utils/log"
)

type SpaceListRespone struct {
	Code      int          `json:"code"`
	Msg       string       `json:"msg"`
	SpaceList []InnerSpace `json:"space_list"`
}

func (slr *SpaceListRespone) Init(spaceList *SpaceList) {
	slr.SpaceList = make([]InnerSpace, 0)
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
	Code int32  `json:"code"`
	Msg  string `json:"msg"`
}

type PsCreateSpaceRequest struct {
	Space        InnerSpace `json:"space"`
	CurPartition Partition  `json:"cur_partition"`
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
	Space InnerSpace `json:"space"`
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
	DbName    string `json:"db_name"`
	SpaceName string `json:"space_name"`
}
type CommonResponse struct {
	Status Status `json:"status"`
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

func CheckApiSpace(s *ApiSpace) string {
	if err_str := NameValidate(s.DbName); err_str != "" {
		err_str = "DbName 格式错误：" + err_str
		log.Error(err_str)
		return err_str
	}
	if err_str := NameValidate(s.SpaceName); err_str != "" {
		err_str = "Space Name 格式错误：" + err_str
		log.Error(err_str)
		return err_str
	}
	if s.PartitionNum <= 0 || s.PartitionNum > 100 {
		err_str := fmt.Sprintf("PartitionNum[%d] error, num range [1, 100]", s.PartitionNum)
		log.Error(err_str)
		return err_str
	}
	if s.ReplicaNum <= 0 || s.ReplicaNum%2 == 0 || s.ReplicaNum > 5 {
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
	if genericutil.InSlice(s.IdType, cfg.DocKeyTypes) {
		err_str := fmt.Sprintf("CreateSpace api, _id_type only support %v.", cfg.DocKeyTypes)
		log.Error(err_str)
		return err_str
	}
	vector_field_num, scalar_field_num := 0, 0
	indexNameMp, fieldNameMp := map[string]int{}, map[string]int{}
	msg := ""
	for _, field := range s.Fields {
		if len(field.Name) == 0 {
			msg += "Field name is null. "
			continue
		}
		if _, ok := fieldNameMp[field.Name]; ok {
			msg += "Field name:" + field.Name + " is repeated. "
			continue
		}
		fieldNameMp[field.Name] = 1

		if field.Index != nil {
			if len(field.Index.Name) == 0 {
				msg += "Field Index name is null. "
				continue
			}
			if _, ok := indexNameMp[field.Index.Name]; ok {
				msg += "index name:" + field.Index.Name + " is repeated. "
				continue
			}
			indexNameMp[field.Index.Name] = 1
		}

		if genericutil.InSlice(field.Type, cfg.ScalarFieldTypes) {
			if field.Index != nil {
				if genericutil.InSlice(field.Index.Type, cfg.ScalarIndexTypes) == false {
					msg += fmt.Sprintf("scalar index type:"+field.Index.Type+" is failed, it should is %v",
						cfg.ScalarIndexTypes)
					continue
				}
				scalar_field_num += 1
			}
		} else if genericutil.InSlice(field.Type, cfg.VectorFieldTypes) {
			if vector_field_num > 1 {
				msg += fmt.Sprintf("has 2 vector field, now only allow 1 vector field. ")
				continue
			}
			if field.Dimension < 1 || field.Dimension > 65536 {
				msg += fmt.Sprintf("vector Dimension, should in [1, 65536]. ")
				continue
			}
			if field.Index == nil {
				msg += fmt.Sprintf("please set vector index info. ")
				continue
			}
			if genericutil.InSlice(field.Index.Type, cfg.VectorIndexTypes) == false {
				msg += fmt.Sprintf("vector index type:"+field.Index.Type+" is failed, it should is %v. ",
					cfg.VectorIndexTypes)
				continue
			}
			if genericutil.InSlice(field.Index.Params.MetricType, cfg.MetricType) == false {
				msg += fmt.Sprintf("metric_type should is %v. ", cfg.MetricType)
				continue
			}
			if field.Index.Type == "IVFPQ" {
				if field.Index.Params.Ncentroids < 2 || field.Index.Params.Nsubvector < 2 {
					msg += fmt.Sprintf("IVFPQ index, ncentroids and nsubvector should be setted. ")
					continue
				}
				hnswParam := field.Index.Params.Hnsw
				if hnswParam != nil {
					if hnswParam.Nlinks < 2 || hnswParam.EfConstruction < 2 ||
						hnswParam.EfSearch < 2 {
						msg += fmt.Sprintf("IVFPQ HNSW index, efSearch and nlinks and efConstruction should be setted. ")
						continue
					}
				}
			} else if field.Index.Type == "HNSW" {
				if field.Index.Params.Nlinks < 2 || field.Index.Params.EfConstruction < 2 ||
					field.Index.Params.EfSearch < 2 {
					msg += fmt.Sprintf("HNSW index, efSearch and nlinks and efConstruction should be setted. ")
					continue
				}
			} else if field.Index.Type == "IVFFLAT" {
				if field.Index.Params.Ncentroids < 2 {
					msg += fmt.Sprintf("IVFFLAT index, ncentroids should be setted. ")
					continue
				}
			} else if field.Index.Type == "VEARCH" {
				if field.Index.Params.Ncentroids < 2 || field.Index.Params.Nsubvector < 2 {
					msg += fmt.Sprintf("VEARCH index, ncentroids and nsubvector should be setted. ")
					continue
				}
			} else if field.Index.Type == "FLAT" {

			} else if field.Index.Type == "BINARYIVF" {
				if field.Index.Params.Ncentroids < 2 {
					msg += fmt.Sprintf("BINARYIVF index, ncentroids should be setted. ")
					continue
				}
			} else {
				msg += "vector index type errr, it is in [IVFPQ, HNSW, IVFFLAT, VEARCH, FLAT, BINARYIVF]; "
				continue
			}
			vector_field_num += 1
		} else {
			msg += fmt.Sprintf("Field type:"+field.Type+" is error, it should is %v or %v. ",
				cfg.ScalarFieldTypes, cfg.VectorFieldTypes)
			continue
		}
	}
	if vector_field_num == 0 {
		msg += "There is no vector field. "
	}
	if len(msg) == 0 {
		fmt.Print("Space data Validate pass\n")
	} else {
		fmt.Print("Space data Validate failed. " + msg)
	}
	return msg
}

func ApiSpaceToInnerSpace(s *ApiSpace) InnerSpace {
	innerSpace := InnerSpace{
		DbName:       s.DbName,
		SpaceName:    s.SpaceName,
		Id:           s.Id,
		PartitionNum: s.PartitionNum,
		ReplicaNum:   s.ReplicaNum,
		Partitions:   s.Partitions,
		IdType:       s.IdType,
		Ctime:        s.Ctime,
		Mtime:        s.Mtime,
	}
	for _, field := range s.Fields {
		innerField := InnerField{
			Name:      field.Name,
			Type:      field.Type,
			Dimension: field.Dimension,
		}
		index := field.Index
		if index != nil {
			innerIndex := InnerIndex{
				Name: index.Name,
				Type: index.Type,
			}
			if index.Params != nil {
				if index.Type == "IVFPQ" || index.Type == "VEARCH" {
					ivfpq := IVFPQ{
						MetricType:        index.Params.MetricType,
						TrainingThreshold: index.Params.TrainingThreshold,
						Ncentroids:        index.Params.Ncentroids,
						Nsubvector:        index.Params.Nsubvector,
						ThreadNum:         index.Params.ThreadNum,
					}
					if index.Params.Hnsw != nil {
						ivfpq.Hnsw = index.Params.Hnsw
					}
					ivfpqJson, _ := genericutil.ToJSON(ivfpq)
					innerIndex.Params = &ivfpqJson
				} else if index.Type == "HNSW" {
					metricType := index.Params.MetricType
					hnsw := HNSW{
						Nlinks:         index.Params.Nlinks,
						EfConstruction: index.Params.EfConstruction,
						EfSearch:       index.Params.EfSearch,
						MetricType:     &metricType,
					}
					hnswJson, _ := genericutil.ToJSON(hnsw)
					innerIndex.Params = &hnswJson
				}
				innerField.Index = &innerIndex
			}
		}
		innerSpace.Fields = append(innerSpace.Fields, innerField)
	}
	return innerSpace
}
