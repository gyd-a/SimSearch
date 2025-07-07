#include "common/brpc_options.h"
#include "utils/log.h"

bool BrpcOptions::Init(const std::string& addr_port,
                       const brpc::ChannelOptions* options) {
  _channel = std::make_unique<brpc::Channel>();
  _addr_port = addr_port;
  if (_channel->Init(addr_port.c_str(), options) != 0) {
    return false;
  }
  _stub.reset(new ps_rpc::PsService_Stub(_channel.get()));
  return true;
}

bool RaftOptions::Build(const brpc::ChannelOptions* options) {
  auto chan = std::make_unique<brpc::Channel>();
  if (chan->Init(leader_.addr, options) != 0) {
    return false;
  }
  auto stub = std::make_unique<raft_rpc::PsRaftService_Stub>(chan.get());
  channel_ = std::move(chan);
  stub_ = std::move(stub);
  return true;
}
