// Copyright 2019 The Vearch Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.

package etcdcluster

import (
	"context"
	"fmt"
	"strconv"
	"time"

	"master/config"

	"go.etcd.io/etcd/api/v3/etcdserverpb"
	clientv3 "go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/client/v3/concurrency"
)


// func init() {
// 	Register("etcd", NewEtcdClient)
// }

type EtcdClient struct {
	//cli is the etcd client
	cli *clientv3.Client
}

// NewIDGenerate create a global uniqueness id
func (sc *EtcdClient) NewIDGenerate(ctx context.Context, key string, base int64) (int64, error) {
	var (
		nextID = int64(0)
		err    error
	)
	err = sc.STM(ctx, func(stm concurrency.STM) error {
		v := stm.Get(key)
		if len(v) == 0 {
			stm.Put(key, fmt.Sprintf("%v", base))
			nextID = base
			return nil
		}

		intv, err := strconv.ParseInt(v, 10, 64)
		if err != nil {
			return fmt.Errorf("increment id error in storage :%v", v)
		}

		nextID = intv + 1
		stm.Put(key, strconv.FormatInt(nextID, 10))
		return nil
	})

	if err != nil {
		return int64(0), err
	}
	return nextID, nil
}

// func (sc *EtcdClient) NewLock(ctx context.Context, key string, timeout time.Duration) *DistLock {
// 	return NewDistLock(ctx, sc.cli, key, timeout)
// }

// NewEtcdClient is used to register etcd sc init function
func NewEtcdClient(serverAddrs []string) (*EtcdClient, error) {
	var cli *clientv3.Client
	var err error
	if config.Conf().Global.SupportEtcdAuth {
		cli, err = clientv3.New(clientv3.Config{
			Endpoints:   serverAddrs,
			DialTimeout: 5 * time.Second,
			Username:    config.Conf().EtcdConfig.Username,
			Password:    config.Conf().EtcdConfig.Password,
		})
	} else {
		cli, err = clientv3.New(clientv3.Config{
			Endpoints:   serverAddrs,
			DialTimeout: 5 * time.Second,
		})
	}
	if err != nil {
		return nil, err
	}
	return &EtcdClient{cli: cli}, nil
}

func (sc *EtcdClient) GetCli() *clientv3.Client {
	return sc.cli
}

// put kv if already exits it will overwrite
func (sc *EtcdClient) Put(ctx context.Context, key string, value []byte) error {
	_, err := sc.cli.Put(ctx, key, string(value))
	return err
}

// if key already in , it will check version  if same insert else ?????
// if key is not in , it will put
func (sc *EtcdClient) Create(ctx context.Context, key string, value []byte) error {
	resp, err := sc.cli.Txn(ctx).
		If(clientv3.Compare(clientv3.Version(key), "=", 0)).
		Then(clientv3.OpPut(key, string(value))).
		Commit()
	if err != nil {
		return err
	}
	if !resp.Succeeded {
		return fmt.Errorf("etcd store key :%v error", key)
	}
	return nil
}

// CreateWithTTL will create the key-value
// if key already in , it will overwrite
// if key is not in , it will put
func (sc *EtcdClient) CreateWithTTL(ctx context.Context, key string, value []byte, ttl time.Duration) error {
	if ttl != 0 && int64(ttl.Seconds()) == 0 {
		return fmt.Errorf("ttl time must greater than 1 second")
	}

	grant, err := sc.cli.Grant(ctx, int64(ttl.Seconds()))
	if err != nil {
		return err
	}

	_, err = sc.cli.Put(ctx, key, string(value), clientv3.WithLease(grant.ID))
	return err
}

func (sc *EtcdClient) KeepAlive(ctx context.Context, key string, value []byte, ttl time.Duration) (<-chan *clientv3.LeaseKeepAliveResponse, error) {
	if ttl != 0 && int64(ttl.Seconds()) == 0 {
		return nil, fmt.Errorf("ttl time must greater than 1 second")
	}

	grant, err := sc.cli.Grant(ctx, int64(ttl.Seconds()))
	if err != nil {
		return nil, err
	}
	_, err = sc.cli.Put(ctx, key, string(value), clientv3.WithLease(grant.ID))
	if err != nil {
		return nil, err
	}

	keepaliveC, err := sc.cli.KeepAlive(ctx, grant.ID)
	if err != nil {
		return nil, err
	}

	return keepaliveC, err
}

func (sc *EtcdClient) PutWithLeaseId(ctx context.Context, key string, value []byte, ttl time.Duration, leaseId clientv3.LeaseID) error {
	if ttl != 0 && int64(ttl.Seconds()) == 0 {
		return fmt.Errorf("ttl time must greater than 1 second")
	}

	_, err := sc.cli.Put(ctx, key, string(value), clientv3.WithLease(leaseId))
	if err != nil {
		return err
	}

	return nil
}

func (sc *EtcdClient) Update(ctx context.Context, key string, value []byte) error {
	_, err := sc.cli.Put(ctx, key, string(value))
	return err
}

func (sc *EtcdClient) Get(ctx context.Context, key string) ([]byte, error) {
	resp, err := sc.cli.Get(ctx, key)
	if err != nil {
		return nil, err
	}
	if len(resp.Kvs) < 1 {
		return nil, nil
	}

	return resp.Kvs[0].Value, nil
}

func (sc *EtcdClient) PrefixScan(ctx context.Context, prefix string) ([][]byte, [][]byte, error) {
	resp, err := sc.cli.Get(ctx, prefix, clientv3.WithPrefix())
	if err != nil {
		return nil, nil, err
	}

	vale := make([][]byte, len(resp.Kvs))
	keys := make([][]byte, len(resp.Kvs))
	for i, v := range resp.Kvs {
		vale[i] = v.Value
		keys[i] = v.Key
	}
	return keys, vale, nil
}

func (sc *EtcdClient) Delete(ctx context.Context, key string) error {
	resp, err := sc.cli.Delete(ctx, key)
	if err != nil {
		return fmt.Errorf("failed to delete %s from etcd store, the error is :%s", key, err.Error())
	}
	if resp.Deleted != 1 {
		return fmt.Errorf("key not exist error, key:%v", key)
	}
	return nil
}

func (sc *EtcdClient) STM(ctx context.Context, apply func(stm concurrency.STM) error) error {
	resp, err := concurrency.NewSTM(sc.cli, apply)
	if err != nil {
		return err
	}
	if !resp.Succeeded {
		return fmt.Errorf("etcd stm failed")
	}

	return nil
}

func (sc *EtcdClient) WatchPrefix(ctx context.Context, key string) (clientv3.WatchChan, error) {
	// startRevision := int64(0)
	// initial, err := sc.cli.Get(ctx, key)
	// if err == nil {
	// 	startRevision = initial.Header.Revision
	// }
	// watcher := sc.cli.Watch(ctx, key, clientv3.WithPrefix(), clientv3.WithRev(startRevision))
	watcher := sc.cli.Watch(ctx, key, clientv3.WithPrefix())
	if watcher == nil {
		return nil, fmt.Errorf("watch %v failed", key)
	}

	return watcher, nil
}

func (sc *EtcdClient) MemberList(ctx context.Context) (*clientv3.MemberListResponse, error) {
	requestCtx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	return sc.cli.MemberList(requestCtx)
}

func compareEndPoints(eps []string, members []*etcdserverpb.Member) bool {
	memberAddrs := make(map[string]bool)
	for _, member := range members {
		for _, peerURL := range member.PeerURLs {
			memberAddrs[peerURL] = true
		}
	}

	for _, ep := range eps {
		if !memberAddrs[ep] {
			return false
		}
	}

	return true
}

func (sc *EtcdClient) MemberStatus(ctx context.Context) ([]*clientv3.StatusResponse, error) {
	membersResp, err := sc.cli.MemberList(ctx)
	if err != nil {
		return nil, err
	}
	eps := sc.cli.Endpoints()
	if compareEndPoints(eps, membersResp.Members) {
		err := sc.cli.Sync(ctx)
		if err != nil {
			return nil, err
		}
	}
	eps = sc.cli.Endpoints()
	status := make([]*clientv3.StatusResponse, len(eps))

	for i, ep := range eps {
		requestCtx, cancel := context.WithTimeout(ctx, 5*time.Second)
		defer cancel()

		stat, err := sc.cli.Status(requestCtx, ep)
		if err != nil {
			status[i] = &clientv3.StatusResponse{
				Header: &etcdserverpb.ResponseHeader{
					ClusterId: 0,
					MemberId:  0,
					Revision:  0,
					RaftTerm:  0,
				},
				Version:          "0",
				DbSizeInUse:      0,
				Leader:           0,
				RaftAppliedIndex: 0,
				Errors:           []string{err.Error()},
			}
		} else {
			status[i] = stat
		}
	}

	return status, nil
}

func (sc *EtcdClient) MemberAdd(ctx context.Context, peerAddrs []string) (*clientv3.MemberAddResponse, error) {
	return sc.cli.MemberAdd(ctx, peerAddrs)
}

func (sc *EtcdClient) MemberRemove(ctx context.Context, id uint64) (*clientv3.MemberRemoveResponse, error) {
	// TODO also remove persistence data
	return sc.cli.MemberRemove(ctx, id)
}

func (sc *EtcdClient) Endpoints() []string {
	return sc.cli.Endpoints()
}

func (sc *EtcdClient) MemberSync(ctx context.Context) error {
	return sc.cli.Sync(ctx)
}
