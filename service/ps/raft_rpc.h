#pragma once

#include <brpc/controller.h>      // brpc::Controller
#include <brpc/server.h>          // brpc::Server
#include <butil/sys_byteorder.h>  // butil::NetToHost32
#include <fcntl.h>                // open
#include <sys/types.h>            // O_CREAT

#include <mutex>

#include "config/conf.h"
#include "idl/gen_idl/rpc_service_idl/raft_rpc.pb.h"
#include "raft_store/raft_store.h"

class RaftRpcImpl : public raft_rpc::PsRaftService {
 public:
  // explicit
  RaftRpcImpl() {}

  void write(::google::protobuf::RpcController* controller,
             const raft_rpc::BlockRequest* request,
             raft_rpc::BlockResponse* response,
             ::google::protobuf::Closure* done) override;

  void read(::google::protobuf::RpcController* controller,
            const raft_rpc::BlockRequest* request,
            raft_rpc::BlockResponse* response,
            ::google::protobuf::Closure* done) override;

 private:
};

/*
class RaftRpcImpl : public raft_rpc::PsRaftService {
 public:
  //explicit
  RaftRpcImpl(Block* block) : _block(block) {}

  void write(::google::protobuf::RpcController* controller,
             const raft_rpc::BlockRequest* request,
             raft_rpc::BlockResponse* response,
             ::google::protobuf::Closure* done) override;

  void read(::google::protobuf::RpcController* controller,
            const raft_rpc::BlockRequest* request,
            raft_rpc::BlockResponse* response,
            ::google::protobuf::Closure* done) override;

  void InsertData(::google::protobuf::RpcController* controller,
                  const common_rpc::InsertDataRequest* req,
                  common_rpc::InsertDataResponse* resp,
                  ::google::protobuf::Closure* done) override;

  void DeleteData(::google::protobuf::RpcController* controller,
                  const common_rpc::DeleteDataRequest* req,
                  common_rpc::DeleteDataResponse* resp,
                  ::google::protobuf::Closure* done) override;

 private:
  Block* _block;
};
*/

class TestDone : public ::google::protobuf::Closure {
 public:
  void Run();
};
