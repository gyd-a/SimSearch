package httputil

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"time"
)

// PostRequest sends a POST request with raw JSON bytes and unmarshals the response into respStruct
func PostRequest(url string, headers map[string]string, requestBody []byte,
	respStruct interface{}) (resp_str string, e_str string) {
	// 创建请求
	req, err := http.NewRequest("POST", url, bytes.NewBuffer(requestBody))
	if err != nil {
		return "", "failed to build http request: " + err.Error()
	}
	// 设置默认 Content-Type 和用户自定义的 header
	req.Header.Set("Content-Type", "application/json")
	for k, v := range headers {
		req.Header.Set(k, v)
	}
	// 发起请求
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return "", "failed to send http request: " + err.Error()
	}
	defer resp.Body.Close()
	// 读取响应
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", "failed to read http response body: " + err.Error()
	}
	// 反序列化 JSON 响应
	if err := json.Unmarshal(body, respStruct); err != nil {
		return string(body), "failed to unmarshal http response body: " + err.Error()
	}
	return string(body), ""
}
