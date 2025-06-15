package etcdcluster

import (
	"context"
	"fmt"
	"master/utils/log"
	"time"

	clientv3 "go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/client/v3/concurrency"
)

type EtcdLocker struct {
	Client  *clientv3.Client
	Session *concurrency.Session
	Mutex   *concurrency.Mutex
	Key     string
}

// 创建分布式锁
func GetEtcdLocker(client *clientv3.Client, lockKey string) (*EtcdLocker, error) {
	session, err := concurrency.NewSession(client)
	if err != nil {
		return nil, fmt.Errorf("创建 session 失败: %w", err)
	}

	mutex := concurrency.NewMutex(session, lockKey)

	return &EtcdLocker{
		Session: session,
		Mutex:   mutex,
		Key:     lockKey,
	}, nil
}

func (l *EtcdLocker) TryLock(ctx context.Context, retry int, intervalMs int) error {
	for i := 0; i < retry; i++ {
		err := l.Mutex.TryLock(ctx)
		if err == nil {
			log.Infof("tryLock key[%s] success.", l.Key)
			return nil
		}
		log.Infof("tryLock key[%s] failed: %v (retry %d/%d)", l.Key, err, i+1, retry)
		// 等待再重试，最后一次不等待
		if i < retry-1 {
			time.Sleep(time.Duration(intervalMs) * time.Millisecond)
		}
	}
	log.Errorf("tryLock key[%s] failed after %d retries", l.Key, retry)
	return fmt.Errorf("tryLock key failed after %d retries", retry)
}

// 解锁
func (l *EtcdLocker) Unlock(ctx context.Context) error {
	if err := l.Mutex.Unlock(ctx); err != nil {
		log.Infof("unlock key[%s] failed: %w", l.Key, err)
		return fmt.Errorf("unlock key failed, %w", err)
	}
	log.Infof("unlock key[%s] success.", l.Key)
	return nil
}

// 关闭资源
func (l *EtcdLocker) Close() {
	if l.Session != nil {
		l.Session.Close()
	}
}

func (l *EtcdLocker) UnlockAndClose(ctx context.Context) error {
	if err := l.Unlock(ctx); err != nil {
		return err
	}
	l.Close()
	return nil
}
