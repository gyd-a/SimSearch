package config

import "fmt"

type RouterCfg struct {
	Port          uint16   `toml:"port,omitempty" json:"port"`
	PprofPort     uint16   `toml:"pprof_port,omitempty" json:"pprof_port"`
	RpcPort       uint16   `toml:"rpc_port,omitempty" json:"rpc_port"`
	MonitorPort   uint16   `toml:"monitor_port" json:"monitor_port"`
	ConnLimit     int      `toml:"conn_limit" json:"conn_limit"`
	CloseTimeout  int64    `toml:"close_timeout" json:"close_timeout"`
	RouterIPS     []string `toml:"router_ips" json:"router_ips"`
	ConcurrentNum int      `toml:"concurrent_num" json:"concurrent_num"`
	RpcTimeOut    int      `toml:"rpc_timeout" json:"rpc_timeout"` // ms
}

func (routerCfg *RouterCfg) ApiUrl(keyNumber int) string {
	var Addr string
	if routerCfg.RouterIPS != nil && len(routerCfg.RouterIPS) > 0 && keyNumber < len(routerCfg.RouterIPS) {
		Addr = routerCfg.RouterIPS[keyNumber]
	}
	if routerCfg.Port == 80 {
		return "http://" + Addr
	}
	return fmt.Sprintf("http://%s:%d", Addr, routerCfg.Port)
}
