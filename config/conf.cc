#include "config/conf.h"

#include "iostream"
#include "utils/net.h"
#include <butil/logging.h>
#include <butil/file_util.h>

std::string _cur_IP;

std::string _etcd_space_prefix = "/spaces/mata";
std::string _ps_register_prefix = "/nodes/ps/";          // + "{node_id}_{IP}"
std::string _router_register_prefix = "/nodes/router/";  // + "{IP}:{PORT}"

std::string _router_store_path = "router_storage";
std::string _ps_storage_dir = "ps_storage";
std::string _ps_raft_root_path = "ps_raft_data";

std::string _ps_mata_file = "ps_mata.json";
TomlConfig toml_conf_;

// RaftStoreOptions _raft_opt = RaftStoreOptions();

int InitConfig(const std::string& ip, const std::string& ps_port,
               const std::string& router_port, const std::string& toml_file) {
  std::cout << "Run 'InitConfig()' function. ";
  std::string msg = toml_conf_.Load(toml_file);
  
  if (msg.size() > 0) {
    return -1;
  }
  if (ip.size() > 0) {
    _cur_IP = ip;
  } else {
    if (GetLocalIPAddress(_cur_IP) == false) {
      std::cout << "Failed to get local IP address, process exit()";
      return -1;
    }
  }
  if (ps_port.size() > 0) {
    toml_conf_.ps.rpc_port = atoi(ps_port.c_str());
  }
  if (router_port.size() > 0) {
    toml_conf_.router.port = atoi(router_port.c_str());
  }

  _router_store_path = toml_conf_.global.data[0] + "router_storage";
  _ps_storage_dir = toml_conf_.global.data[0] + "ps_storage";
  _ps_raft_root_path = toml_conf_.global.data[0] + "ps_raft_data";
  butil::CreateDirectory(butil::FilePath(toml_conf_.global.data[0]));

  LOG(INFO) << "Run 'InitConfig()' function. Current local IP address: "
            << _cur_IP << " ps_port:" << toml_conf_.ps.rpc_port;
  return 0;
};
