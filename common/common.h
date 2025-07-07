
#pragma once

#include <string>
#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"

std::string GenSpaceKey(const std::string& db_name,
                        const std::string& space_name);

std::string GetRaftConf(const common_rpc::Partition& pb_partition);

std::pair<std::string, std::string> GetPsRegisterKV(const std::string& IP, int port, int ps_id);

std::string GetPsRegisterKeyPrefixWithIpAndPort(const std::string& IP, int port);

std::string GenRouterNodeKey(const std::string& node_IP, int port);


