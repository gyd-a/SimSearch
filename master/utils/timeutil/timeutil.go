package timeutil

import (
	"fmt"
	"time"
)

// 获取当前时间：格式化字符串 和 毫秒时间戳
func GetCurTime() (string, string, int64) {
	now := time.Now()

	// 格式化时间字符串
	formatted := now.Format("20060102 15:04:05")

	// 获取毫秒时间戳
	milliTimestamp := now.UnixNano() / int64(time.Millisecond)
	timeStrAndMsTs := fmt.Sprintf("%s,%d", formatted, milliTimestamp)
	return timeStrAndMsTs, formatted, milliTimestamp
}

