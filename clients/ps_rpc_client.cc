#include "clients/ps_rpc_client.h"

#include <brpc/channel.h>

#include <string>
#include <vector>

#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include "idl/gen_idl/rpc_service_idl/ps_rpc.pb.h"


template <typename Stub, typename Request, typename Response>
std::string QueryPsRpcImp(ps_rpc::PsService_Stub* stub,
                          void (Stub::*method)(google::protobuf::RpcController*,
                                               const Request*, Response*,
                                               google::protobuf::Closure*),
                          const Request* req, Response* resp,
                          const std::string& op_type) {
  std::string msg;
  brpc::Controller cntl;
  butil::Timer tm;
  tm.start();

  // 调用传入的方法
  (stub->*method)(&cntl, req, resp, nullptr);

  tm.stop();
  if (cntl.Failed()) {
    msg = "Query Ps " + op_type + " rpc failed. error:" + cntl.ErrorText();
    LOG(ERROR) << msg;
    return msg;
  }
  LOG(INFO) << "Received Ps " << op_type << " rpc successful.";
  return msg;
};



std::string PsRpcClient::Init(const std::string& addr_port,
                              const std::string& group_name, int ps_id) {
  _options.protocol = "baidu_std";
  _options.connection_type = "pooled";
  _options.timeout_ms = 500; /*milliseconds*/
  _options.max_retry = 3;
  std::string msg = "";
  if (_brpc_opt.Init(addr_port, &_options) == false) {
    msg = "Fail to initialize channel, addr_and_port:" + addr_port;
    LOG(ERROR) << msg;
    return msg;
  }
  return msg;
}

std::string PsRpcClient::Get(const std::vector<std::string>& keys,
                             common_rpc::GetResponse& resp) {
  common_rpc::GetRequest req;
  for (int i = 0; i < keys.size(); ++i) {
    // req.add_keys(keys[i]);
  }
  // req.set_key_type(1);
  if (_brpc_opt._stub == nullptr) {
    std::string msg =
        "addr_port:" + _brpc_opt._addr_port + " stub is nullptr, not run Get()";
    LOG(ERROR) << msg;
    return msg;
  }
  std::string msg = QueryPsRpcImp(
      _brpc_opt._stub.get(), &ps_rpc::PsService_Stub::Get, &req, &resp, "Get");
  return msg;
}

void PsRpcClient::Search() {
  common_rpc::SearchRequest req;
  common_rpc::SearchResponse resp;
  std::string msg =
      QueryPsRpcImp(_brpc_opt._stub.get(),
                    &ps_rpc::PsService_Stub::Search, &req, &resp, "Search");
  return;
}

std::string PsRpcClient::MockGet(const std::vector<std::string>& keys,
                                 common_rpc::MockGetResponse& resp) {
  if (_brpc_opt._stub == nullptr) {
    std::string msg = "addr_port:" + _brpc_opt._addr_port +
                      " stub is nullptr, not run MockGet()";
    LOG(ERROR) << msg;
    return msg;
  }
  common_rpc::MockGetRequest req;
  for (int i = 0; i < keys.size(); ++i) {
    req.add_keys(keys[i]);
  }
  req.set_key_type(1);
  std::string msg =
      QueryPsRpcImp(_brpc_opt._stub.get(), &ps_rpc::PsService_Stub::MockGet,
                    &req, &resp, "MockGet");
  return msg;
}
