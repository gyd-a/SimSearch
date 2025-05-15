#pragma once

#include <brpc/server.h>          // brpc::Server
#include "service/router/router_rpc.h"

class RouterServer {
 public:
  RouterServer() {}
  ~RouterServer() {}

  bool Start();

  void WaitBrpcStop();

 private:
  brpc::Server _server;
  RouterSpaceRpcImpl _space_impl;
  RouterDocRpcImpl _doc_impl;
};
