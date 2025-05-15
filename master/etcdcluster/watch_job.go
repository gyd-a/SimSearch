package etcdcluster

import (
	"context"
	"fmt"
	"strings"
	"master/config"
	"master/utils/log"
	clientv3 "go.etcd.io/etcd/client/v3"

)




type WatchJob struct {
	EtcdCli             *EtcdClient
	watchChan          clientv3.WatchChan
}


func (wj *WatchJob) InitWatchJob(etcdClient *EtcdClient) {
	wj.EtcdCli = etcdClient
}

func (wj *WatchJob) Strat() (err error) {
	ctx := context.Background()
	if wj.watchChan, err = wj.EtcdCli.WatchPrefix(ctx, config.NodePrefix); err != nil {
		log.Error("WatchJob StartRouter error: %v", err)
		return err
	}

	fmt.Println("开始监听前缀：", config.NodePrefix)
	for watchResponse := range wj.watchChan {
		for _, event := range watchResponse.Events {
			var nodeType string = "unknown"
			if strings.Contains(string(event.Kv.Key), "router") {
				nodeType = "router"
			} else if strings.Contains(string(event.Kv.Key), "ps") {
				nodeType = "ps"
			}// else {}
			switch event.Type {
			case clientv3.EventTypePut:
				fmt.Printf("节点类型: %s, 上线或修改: %s, 值: %s\n", nodeType, event.Kv.Key, event.Kv.Value)
				log.Info("节点类型: %s, 节点上线或修改: %s, 值: %s\n", nodeType, event.Kv.Key, event.Kv.Value)
			case clientv3.EventTypeDelete:
				fmt.Printf("节点类型: %s, 节点下线: %s\n", nodeType, event.Kv.Key)
				log.Info("节点类型: %s, 节点下线: %s\n", nodeType, event.Kv.Key)
			default:
				fmt.Printf("节点类型: %s, 未识别的事件类型: %v\n", nodeType, event.Type)
				log.Info("节点类型: %s, 未识别的事件类型: %v\n", nodeType, event.Type)
			}
		}
	}
	return err
}

