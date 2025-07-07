#pragma once

#include <brpc/controller.h>      // brpc::Controller
#include <brpc/server.h>          // brpc::Server
#include <butil/sys_byteorder.h>  // butil::NetToHost32
#include <fcntl.h>                // open
#include <sys/types.h>            // O_CREAT
#include <gflags/gflags.h>

#include <atomic>

#include "config/conf.h"
#include "idl/gen_idl/rpc_service_idl/ps_rpc.pb.h"
#include "raft_store/raft_store.h"


// Implements example::BlockService if you are using brpc.
class PsRpcImpl : public ps_rpc::PsService {
 public:
  explicit PsRpcImpl() {}

  void MockGet(::google::protobuf::RpcController* controller,
               const ::common_rpc::MockGetRequest* req,
               ::common_rpc::MockGetResponse* resp,
               ::google::protobuf::Closure* done) override;

  void Ping(::google::protobuf::RpcController* controller,
            const ps_rpc::PingRequest* req, ps_rpc::PingResponse* resp,
            ::google::protobuf::Closure* done) override;

  void CreateSpace(::google::protobuf::RpcController* controller,
                   const ::ps_rpc::CreateSpaceRequest* req,
                   ::ps_rpc::CreateSpaceResponse* resp,
                   ::google::protobuf::Closure* done) override;

  void DeleteSpace(::google::protobuf::RpcController* controller,
                   const ::common_rpc::DeleteSpaceRequest* req,
                   ::common_rpc::DeleteSpaceResponse* resp,
                   ::google::protobuf::Closure* done) override;

  void Get(::google::protobuf::RpcController* controller,
           const ::common_rpc::GetRequest* req, ::common_rpc::GetResponse* resp,
           ::google::protobuf::Closure* done) override;

  void Search(::google::protobuf::RpcController* controller,
              const ::common_rpc::SearchRequest* req,
              ::common_rpc::SearchResponse* resp,
              ::google::protobuf::Closure* done) override;

 private:
};

// class TestDone : public ::google::protobuf::Closure {
//  public:
//   void Run();
// };
