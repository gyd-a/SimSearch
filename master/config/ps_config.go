package config

type PSCfg struct {
	RpcPort                     uint16 `toml:"rpc_port,omitempty" json:"rpc_port"`
	PsHeartbeatTimeout          int    `toml:"ps_heartbeat_timeout" json:"ps_heartbeat_timeout"`
	RaftHeartbeatPort           uint16 `toml:"raft_heartbeat_port,omitempty" json:"raft_heartbeat_port"`
	RaftReplicatePort           uint16 `toml:"raft_replicate_port,omitempty" json:"raft_replicate_port"`
	RaftHeartbeatInterval       int    `toml:"heartbeat_interval" json:"heartbeat-interval"`
	RaftRetainLogs              uint64 `toml:"raft_retain_logs" json:"raft-retain-logs"`
	RpcTimeOut                  int    `toml:"rpc_timeout" json:"rpc_timeout"`
}
