package main

import (
	"flag"
	"fmt"
	"master/config"
	"master/etcdcluster"
	"master/utils/log"
	"master/utils/slog"
	"master/clusterapi"
	"os"
	// "os/signal"
	// "syscall"
	// "time"
)





func main() {
	conf_path := flag.String("conf", "conf.toml", "master服务的配置")
	flag.Parse()
	config.InitConfig(*conf_path)

	slog.SetConfig(config.Conf().GetLogFileNum(), 1024 * 1024 * config.Conf().GetLogFileSize())
	log.Regist(slog.NewSLog(config.Conf().GetLogDir(), "master", config.Conf().GetLevel(), false))

	log.Info("master conf_path: %s", *conf_path)

	if err := config.Conf().CurrentByMasterNameDomainIp("master"); err != nil {
		log.Error("CurrentByMasterNameDomainIp master error: %v, exit()", err)
		os.Exit(1)
	}

	etcdHandler := etcdcluster.EtcdHandler{}
	etcdHandler.StartEmbeddedEtcd(config.Conf())
	fmt.Printf("etcdHandler.StartEmbeddedEtcd success")

	etcdClient, e := etcdcluster.NewEtcdClient(config.Conf().GetEtcdAddress())
	if e != nil {
		log.Error("NewEtcdClient error: %v", e)
		os.Exit(1)
	} else {
		fmt.Printf("etcdClient success")
	}

	clusterApi := clusterapi.ClusterAPI{}
	clusterApi.InitClusterApi(etcdClient)

	watchJob := etcdcluster.WatchJob{EtcdCli : etcdClient}
	watchJob.Strat()
	// 等待退出信号以清理资源
	// 使用信号通知系统来优雅地停止etcd服务
	// sigChan := make(chan os.Signal, 1)
	// signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	// // 等待退出信号
	// sig := <-sigChan
	// fmt.Printf("Received signal %s, shutting down etcd...\n", sig)

	// time.Sleep(1000000000 * time.Second)
}
