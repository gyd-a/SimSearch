#include <butil/file_util.h>

#include "config/conf.h"
#include "iostream"
#include "utils/net.h"
#include "utils/log.h"


std::string _cur_IP;

std::string _etcd_space_prefix = "/spaces/mata";
std::string _ps_register_prefix = "/nodes/ps/";          // + "{node_id}_{IP}"
std::string _router_register_prefix = "/nodes/router/";  // + "{IP}:{PORT}"

std::string _data_dir;

std::string _router_store_dir;
std::string _ps_storage_dir;
std::string _raft_root_dir;
std::string _engine_data_dir;


std::string _ps_mata_file = "ps_mata.json";
TomlConfig toml_conf_;

std::string _log_dir;

// RaftStoreOptions _raft_opt = RaftStoreOptions();

int InitConfig(const std::string& ip, const std::string& ps_port,
               const std::string& router_port, const std::string& toml_file,
               const std::string& log_dir) {
  LOG(INFO) << "Run 'InitConfig()' function. ";
  std::string msg = toml_conf_.Load(toml_file);
  _log_dir = log_dir;
  
  if (msg.size() > 0) {
    return -1;
  }
  if (ip.size() > 0) {
    _cur_IP = ip;
  } else {
    if (GetLocalIPAddress(_cur_IP) == false) {
      LOG(INFO)  << "Failed to get local IP address, process exit()";
      return -1;
    }
  }
  if (ps_port.size() > 0) {
    toml_conf_.ps.rpc_port = atoi(ps_port.c_str());
  }
  if (router_port.size() > 0) {
    toml_conf_.router.port = atoi(router_port.c_str());
  }

  _data_dir = toml_conf_.global.data[0];
  _router_store_dir = _data_dir + "router_storage";
  _ps_storage_dir = _data_dir + "ps_storage";
  _raft_root_dir = _data_dir + "raft_data";
  _engine_data_dir = _data_dir + "engine_data";
  butil::CreateDirectory(butil::FilePath(toml_conf_.global.data[0]));

  LOG(INFO) << "Run 'InitConfig()' function. Current local IP address: "
            << _cur_IP << " ps_port:" << toml_conf_.ps.rpc_port;
  return 0;
};
