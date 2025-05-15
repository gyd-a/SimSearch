package config

import "fmt"



type MasterCfg struct {
	Name           string `toml:"name,omitempty" json:"name"`
	Address        string `toml:"address,omitempty" json:"address"`
	ApiPort        uint16 `toml:"api_port,omitempty" json:"api_port"`
	EtcdPort       uint16 `toml:"etcd_port,omitempty" json:"etcd_port"`
	EtcdPeerPort   uint16 `toml:"etcd_peer_port,omitempty" json:"etcd_peer_port"`
	EtcdClientPort uint16 `toml:"etcd_client_port,omitempty" json:"etcd_client_port"`
	Self           bool   `json:"-"`
	MonitorPort    uint16 `toml:"monitor_port" json:"monitor_port"`
}

func (m *MasterCfg) ApiUrl() string {
	if m.ApiPort == 80 {
		return "http://" + m.Address
	}
	return fmt.Sprintf("http://%s:%d", m.Address, m.ApiPort)
}


type Masters []*MasterCfg

// new client use this function to get client urls
func (ms Masters) ClientAddress() []string {
	addrs := make([]string, len(ms))
	for i, m := range ms {
		addrs[i] = fmt.Sprintf("%s:%d", m.Address, ms[i].EtcdClientPort)
	}
	return addrs
}

func (ms Masters) Self() *MasterCfg {
	for _, m := range ms {
		if m.Self {
			return m
		}
	}
	return nil
}


func MastersEqual(a, b Masters) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if !AreMasterCfgsEqual(*a[i], *b[i]) {
			return false
		}
	}
	return true
}

func AreMasterCfgsEqual(a, b MasterCfg) bool {
	if a.Address != b.Address || a.Name != b.Name {
		return false
	}
	return true
}