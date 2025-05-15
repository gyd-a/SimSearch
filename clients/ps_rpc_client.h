#pragma once

#include <brpc/channel.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "common/brpc_options.h"
#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include "idl/gen_idl/rpc_service_idl/ps_rpc.pb.h"

class PsRpcClient {
 public:
  PsRpcClient() {}

  std::string Init(const std::string& addr_port, const std::string& group_name,
                   int ps_id);

  std::string Get(const std::vector<std::string>& keys,
                  common_rpc::GetResponse& resp);

  void Search();

  std::string MockGet(const std::vector<std::string>& keys,
                      common_rpc::MockGetResponse& resp);

  PsRpcClient(PsRpcClient&&) = default;
  PsRpcClient& operator=(PsRpcClient&&) = default;

 private:
  brpc::ChannelOptions _options;
  BrpcOptions _brpc_opt;
};
