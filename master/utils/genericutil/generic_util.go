package genericutil

import (
	"encoding/json"
)

// 泛型函数，T 必须是可比较的类型（== 和 !=）
func InSlice[T comparable](target T, list []T) bool {
	for _, item := range list {
		if item == target {
			return true
		}
	}
	return false
}

// ToJSON 是一个通用的模板函数，接受任何结构体，返回其 JSON 字符串
func ToJSON[T any](data T) (string, error) {
	bytes, err := json.Marshal(data)
	if err != nil {
		return "", err
	}
	return string(bytes), nil
}

func FromJSON[T any](data string, v *T) error {
	return json.Unmarshal([]byte(data), v)
}
