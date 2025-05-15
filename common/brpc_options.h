#pragma once
#include <braft/raft.h>
#include <brpc/channel.h>

#include <atomic>
#include <memory>
#include <string>

#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include "idl/gen_idl/rpc_service_idl/ps_rpc.pb.h"
#include "idl/gen_idl/rpc_service_idl/raft_rpc.pb.h"

struct BrpcOptions {
  std::unique_ptr<brpc::Channel> _channel;
  std::unique_ptr<ps_rpc::PsService_Stub> _stub = nullptr;
  std::string _addr_port;  // = addr:port

  BrpcOptions() : _channel(nullptr), _stub(nullptr) {}
  BrpcOptions(const BrpcOptions&) = delete;
  BrpcOptions& operator=(const BrpcOptions&) = delete;

  // 手动实现移动构造函数
  BrpcOptions(BrpcOptions&& other) noexcept
      : _addr_port(std::move(other._addr_port)),
        _channel(std::move(other._channel)),
        _stub(std::move(other._stub)) {}

  // 手动实现移动赋值运算符
  BrpcOptions& operator=(BrpcOptions&& other) noexcept {
    if (this != &other) {
      _addr_port = std::move(other._addr_port);
      _channel = std::move(other._channel);
      _stub = std::move(other._stub);
    }
    return *this;
  }

  bool Init(const std::string& addr_port, const brpc::ChannelOptions* options);
};

struct RaftOptions {
  std::string group_;
  std::string conf_;
  braft::PeerId leader_;
  std::unique_ptr<brpc::Channel> channel_;
  std::unique_ptr<raft_rpc::PsRaftService_Stub> stub_;
  std::atomic<bool> ok_{false};

  RaftOptions() : channel_(nullptr), stub_(nullptr) {}
  RaftOptions(const RaftOptions&) = delete;
  RaftOptions& operator=(const RaftOptions&) = delete;

  // 手动实现移动构造函数
  RaftOptions(RaftOptions&& other) noexcept
      : group_(std::move(other.group_)),
        conf_(std::move(other.conf_)),
        leader_(std::move(other.leader_)),
        channel_(std::move(other.channel_)),
        stub_(std::move(other.stub_)),
        ok_(other.ok_.load()) {}

  // 手动实现移动赋值运算符
  RaftOptions& operator=(RaftOptions&& other) noexcept {
    if (this != &other) {
      group_ = std::move(other.group_);
      conf_ = std::move(other.conf_);
      leader_ = std::move(other.leader_);
      channel_ = std::move(other.channel_);
      stub_ = std::move(other.stub_);
      ok_.store(other.ok_.load());
    }
    return *this;
  }
  bool Build(const brpc::ChannelOptions* options);
};
