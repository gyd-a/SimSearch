package etcdcluster

import (
	"errors"
	"fmt"
	"master/config"
	"master/utils/log"
	"net/url"
	"time"

	"go.etcd.io/etcd/server/v3/embed"
)

type EtcdHandler struct {
	etcdCfg        *embed.Config
	etcdServer     *embed.Etcd
	
}

func (etcd_h * EtcdHandler) StartEmbeddedEtcd(conf *config.Config)  (err error) {
	done := make(chan error)
	go func() {
		if etcd_h.etcdCfg, err = etcd_h.GetEmbed(conf); err != nil {
			fmt.Printf("Failed to get embed config: %v", err)
			done <- err
			return
		}
		// 启动 etcd
		etcd_h.etcdServer, err = embed.StartEtcd(etcd_h.etcdCfg)
		if err != nil {
			log.Error("Failed to start embedded etcd: %v", err)
		}
		defer etcd_h.etcdServer.Close()

		select {
		case <-etcd_h.etcdServer.Server.ReadyNotify():
			fmt.Print("Server is ready!")
			fmt.Printf("Listening on: %v\n", etcd_h.etcdCfg.ListenClientUrls)
			done <- nil
		case <-time.After(60 * time.Second):
			etcd_h.etcdServer.Server.Stop() // trigger a shutdown
			fmt.Print("Server took too long to start!")
			done <- errors.New("Server took too long to start!")
		}
		<-etcd_h.etcdServer.Err()
	}()
	return <- done
}


// GetEmbed will get or generate the etcd configuration
func (etcd_h * EtcdHandler) GetEmbed(conf *config.Config) (*embed.Config, error) {
	masterCfg := conf.Masters.Self()
	if masterCfg == nil {
		return nil, fmt.Errorf("not found master config")
	}
	cfg := embed.NewConfig()
	cfg.Name = masterCfg.Name
	cfg.Dir = conf.GetDataDir()
	cfg.WalDir = ""
	cfg.EnablePprof = false
	cfg.StrictReconfigCheck = true
	cfg.TickMs = uint(100)
	cfg.ElectionMs = uint(3000)
	cfg.AutoCompactionMode = "periodic"
	cfg.AutoCompactionRetention = "1"
	cfg.MaxRequestBytes = 33554432
	cfg.QuotaBackendBytes = 8589934592
	cfg.InitialClusterToken = conf.Global.Name
	cfg.LogOutputs = []string{conf.GetLogDir() + "/MASTER.ETCD.log"}
	cfg.LogLevel = "warn"
	cfg.EnableLogRotation = true
	// cfg.ClusterState = "new"
	//set init url
	cfg.InitialCluster = ""
	for _, m := range conf.Masters {
		cfg.InitialCluster += fmt.Sprintf("%s=http://%s:%d,", m.Name, m.Address, masterCfg.EtcdPeerPort)
	}
	cfg.InitialCluster = cfg.InitialCluster[: len(cfg.InitialCluster)-1]
	log.Info("etcd initial cluster is %s", cfg.InitialCluster)

	urlAddr, _ := url.Parse(fmt.Sprintf("http://%s:%d", masterCfg.Address, masterCfg.EtcdPeerPort))
	cfg.ListenPeerUrls = []url.URL{*urlAddr}
	cfg.AdvertisePeerUrls = []url.URL{*urlAddr}

	urlAddr, _ = url.Parse(fmt.Sprintf("http://%s:%d", masterCfg.Address, masterCfg.EtcdClientPort))
	cfg.ListenClientUrls = []url.URL{*urlAddr}
	cfg.AdvertiseClientUrls = []url.URL{*urlAddr}
	return cfg, nil
}