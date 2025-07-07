package entity

type InnerIndex struct {
	Name   string  `json:"name"`
	Type   string  `json:"type"`
	Params *string `json:"params,omitempty"`
}

type ApiIndex struct {
	Name   string       `json:"name"`
	Type   string       `json:"type"`
	Params *IndexParams `json:"params,omitempty"`
}

type IndexParams struct {
	MetricType        string `json:"metric_type"`
	TrainingThreshold int32  `json:"training_threshold,omitempty"`
	Ncentroids        int32  `json:"ncentroids,omitempty"`
	Nsubvector        int32  `json:"nsubvector,omitempty"`
	Nlinks            int32  `json:"nlinks,omitempty"`
	EfConstruction    int32  `json:"efConstruction,omitempty"`
	EfSearch          int32  `json:"efSearch,omitempty"`
	ThreadNum         int32  `json:"thread_num,omitempty"`
	Hnsw              *HNSW  `json:"hnsw,omitempty"`
}

type HNSW struct {
	MetricType     *string `json:"metric_type,omitempty"`
	Nlinks         int32 `json:"nlinks"`
	EfConstruction int32 `json:"efConstruction"`
	EfSearch       int32 `json:"efSearch"`
}

// IVFPQ, IVFHNSWPQ, SCANN
type IVFPQ struct {
	MetricType        string `json:"metric_type"`
	TrainingThreshold int32  `json:"training_threshold,omitempty"`
	Ncentroids        int32  `json:"ncentroids"`
	Nsubvector        int32  `json:"nsubvector"`
	ThreadNum         int32  `json:"thread_num,omitempty"`
	Hnsw              *HNSW  `json:"hnsw,omitempty"`
}

// IVFFLAT, BINARYIVF
type IVF struct {
	MetricType        string `json:"metric_type"`
	TrainingThreshold int32  `json:"training_threshold,omitempty"`
	Ncentroids        int32  `json:"ncentroids"`
}
