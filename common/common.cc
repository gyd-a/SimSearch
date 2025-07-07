#include "common/common.h"
#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include <sstream>
#include <config/conf.h>
#include "utils/log.h"

std::string GenSpaceKey(const std::string& db_name,
                        const std::string& space_name) {
  return db_name + "-" + space_name;
}


std::string GetRaftConf(const common_rpc::Partition& pb_partition) {
  std::string conf;
  const auto& replicas = pb_partition.replicas();
  for (int i = 0; i < replicas.size(); ++i) {
    conf += (replicas[i].ps_ip() + ":" + std::to_string(replicas[i].ps_port()) +
             ":0,");
  }
  return conf;
}

std::pair<std::string, std::string> GetPsRegisterKV(const std::string& IP, int port, int ps_id) {
  std::ostringstream node_key_oss;
  node_key_oss << _ps_register_prefix << ":" << IP << ":" << port << ":" << ps_id;
  return {node_key_oss.str(), std::to_string(port)};
}


std::string GetPsRegisterKeyPrefixWithIpAndPort(const std::string& IP, int port) {
  std::ostringstream node_key_oss;
  node_key_oss << _ps_register_prefix << ":" << IP << ":" << port << ":";
  return node_key_oss.str();
}

std::string GenRouterNodeKey(const std::string& node_IP, int port) {
  std::ostringstream node_key_oss;
  node_key_oss << _router_register_prefix << ":" << node_IP << ":" << port;
  return node_key_oss.str();
};
