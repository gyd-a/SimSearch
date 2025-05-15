

// config_struct.h
#pragma once
#include <string>
#include <vector>
#include <sstream>


struct GlobalConfig {
  std::string name;
  std::vector<std::string> data;
  std::string level;
  std::string signkey;
  bool skip_auth;
  std::string to_string() {
    std::ostringstream ss;
    ss << "GlobalConfig=[name:" << name << ", skip_auth:" << skip_auth
       << ", data:";
    for (int i = 0; i < data.size(); ++i) {
      ss << data[i] << ", ";
    }
    ss << "skip_auth:" << skip_auth << "] ";
    return ss.str();
  }
};

struct MasterConfig {
  std::string name;
  std::string address;
  int api_port;
  int etcd_port;
  int etcd_peer_port;
  int etcd_client_port;
  std::string to_string() {
    std::ostringstream ss;
    ss << "MasterConfig=[name:" << name << ", address:" << address
       << ", api_port:" << api_port << ", etcd_port:" << etcd_port 
       << ", etcd_peer_port:" << etcd_peer_port << ", etcd_client_port"
       << etcd_client_port << "]";
    return ss.str();
  }
};

struct RouterConfig {
  int port;
  bool skip_auth;
  int hearbeat_interval_s;
  std::string to_string() {
    std::ostringstream ss;
    ss << "RouterConfig=[port:" << port << ", skip_auth:" << skip_auth
       << ", hearbeat_interval_s:" << hearbeat_interval_s << "] ";
    return ss.str();
  }
};

struct PsConfig {
  int rpc_port;
  int raft_election_timeout_ms;
  int raft_snap_interval_s;
  int hearbeat_interval_s;
  std::string to_string() {
    std::ostringstream ss;
    ss << "PsConfig=[rpc_port:" << rpc_port << ", raft_election_timeout_ms:"
       << raft_election_timeout_ms << ", raft_snap_interval_s:"
       << raft_snap_interval_s << ", hearbeat_interval_s:"
       << hearbeat_interval_s << "] ";
    return ss.str();
  }
};

struct TomlConfig {
  GlobalConfig global;
  std::vector<MasterConfig> masters;
  RouterConfig router;
  PsConfig ps;

  std::string to_string() {
    std::string msg = global.to_string() + router.to_string() + ps.to_string();
    for (int i = 0; i < masters.size(); ++i) {
      msg += masters[i].to_string();
    }
    return msg;
  }

  std::vector<std::string> master_urls; // {"ip:port"}


  std::string Load(const std::string& filepath);
  std::vector<std::pair<std::string, int>> GetMasterIpPorts();
};
