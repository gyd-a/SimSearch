package concurrent

import (
	"sync"
)

func ParallelExecuteWithResults[T any, R any](
	fn func(T) (R, error) /*  */, argsList []T, maxConcurrency int,
) ([]R, []error) {
	var wg sync.WaitGroup
	taskChan := make(chan int, len(argsList)) // 改为发送索引
	results := make([]R, len(argsList))
	errors := make([]error, len(argsList))
	// 启动worker
	for i := 0; i < maxConcurrency; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for idx := range taskChan {
				res, err := fn(argsList[idx])
				results[idx] = res
				errors[idx] = err
			}
		}()
	}
	// 分发任务(发送索引)
	for idx := range argsList {
		taskChan <- idx
	}
	close(taskChan)
	wg.Wait() // 等待所有worker完成
	return results, errors
}
