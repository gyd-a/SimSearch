#pragma once

#include "idl/gen_idl/rpc_service_idl/router_rpc.pb.h"

// Implements example::BlockService if you are using brpc.
class RouterSpaceRpcImpl : public router_rpc::RouterSpaceService {
 public:
  explicit RouterSpaceRpcImpl() {}

  void AddSpace(::google::protobuf::RpcController* controller,
                const router_rpc::AddSpaceRequest* req,
                router_rpc::AddSpaceResponse* resp,
                ::google::protobuf::Closure* done) override;

  void DeleteSpace(::google::protobuf::RpcController* controller,
                   const common_rpc::DeleteSpaceRequest* req,
                   common_rpc::DeleteSpaceResponse* resp,
                   ::google::protobuf::Closure* done) override;

  void SpaceList(::google::protobuf::RpcController* controller,
                 const ::google::protobuf::Empty* req,
                 router_rpc::LocalSpaces* resp,
                 ::google::protobuf::Closure* done) override;

 private:
};

// Implements example::BlockService if you are using brpc.
class RouterDocRpcImpl : public router_rpc::RouterDocService {
 public:
  explicit RouterDocRpcImpl() {}

  void Get(::google::protobuf::RpcController* controller,
           const common_rpc::GetRequest* req, common_rpc::GetResponse* resp,
           ::google::protobuf::Closure* done) override;

  void Search(::google::protobuf::RpcController* controller,
              const common_rpc::SearchRequest* req,
              common_rpc::SearchResponse* resp,
              ::google::protobuf::Closure* done) override;

  void Upsert(::google::protobuf::RpcController* controller,
              const common_rpc::UpsertRequest* req,
              common_rpc::UpsertResponse* resp,
              ::google::protobuf::Closure* done) override;

  void Delete(::google::protobuf::RpcController* controller,
              const common_rpc::DeleteRequest* req,
              common_rpc::DeleteResponse* resp,
              ::google::protobuf::Closure* done) override;

  void MockUpsert(::google::protobuf::RpcController* controller,
                  const common_rpc::MockUpsertRequest* req,
                  common_rpc::MockUpsertResponse* resp,
                  ::google::protobuf::Closure* done) override;

  void MockGet(::google::protobuf::RpcController* controller,
               const common_rpc::MockGetRequest* req,
               common_rpc::MockGetResponse* resp,
               ::google::protobuf::Closure* done) override;

 private:
};

// class TestDone : public ::google::protobuf::Closure {
//  public:
//   void Run();
// };
