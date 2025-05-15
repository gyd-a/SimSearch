#pragma once


#include "raft_store/raft_store.h"
#include "service/ps/ps_rpc.h"
#include "service/ps/raft_rpc.h"

class BrpcServer {
 public:
  BrpcServer() {}
  ~BrpcServer() {}

  std::string Start();
  
  void WaitBrpcStop();

 private:
  brpc::Server _server;
  PsRpcImpl _ps_impl;
  RaftRpcImpl _raft_impl;
  // RaftServer raft_sv_;
};
