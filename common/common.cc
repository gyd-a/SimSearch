#include "common/common.h"
#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"

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

