package etcdcluster


import (
	"master/config"
)

type Storager struct {
	StoreCli *StoreClient
}

func (st *Storager) initMasterClient(conf *config.Config) (err error) {
	// st.StoreCli, err = OpenStore("etcd", conf.GetEtcdAddress())
	// if err != nil {
	// 	return err
	// }
	return err
}



