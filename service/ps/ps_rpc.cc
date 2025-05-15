

#include "service/ps/ps_rpc.h"

#include <braft/raft.h>           // braft::Node braft::StateMachine
#include <braft/util.h>           // braft::AsyncClosureGuard
#include <brpc/controller.h>      // brpc::Controller
#include <brpc/server.h>          // brpc::Server
#include <butil/sys_byteorder.h>  // butil::NetToHost32
#include <fcntl.h>                // open
#include <sys/types.h>            // O_CREAT

#include <string>

#include "config/conf.h"
#include "engine/engine.h"
#include "raft_store/raft_store.h"
#include "service/ps/local_storager.h"

void PsRpcImpl::MockGet(::google::protobuf::RpcController* controller,
                        const ::common_rpc::MockGetRequest* req,
                        ::common_rpc::MockGetResponse* resp,
                        ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /PsRaftService/GockGet api ================";
  DLOG(INFO) << "====== request: " << req->DebugString() << " ===========";
  auto& engine = Engine::GetInstance();
  const auto& keys = req->keys();
  for (int i = 0; i < keys.size(); ++i) {
    const std::string& k = keys[i];
    int32_t int_key = atoi(k.c_str());
    std::string val = engine.Get(int_key);
    auto doc = resp->add_docs();
    doc->set_key(k);
    doc->set_val(val);
    DLOG(INFO) << "Engine Get(), key:" << k << ", val:" << val;
  }
  resp->set_code(0);
  resp->set_msg("");
  done->Run();
}

void PsRpcImpl::Ping(::google::protobuf::RpcController* controller,
                     const ps_rpc::PingRequest* req, ps_rpc::PingResponse* resp,
                     ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /PsRaftService/Ping api ================";
  resp->mutable_status()->set_msg("success");
  resp->mutable_status()->set_code(0);
  done->Run();
}

void PsRpcImpl::CreateSpace(::google::protobuf::RpcController* controller,
                            const ps_rpc::CreateSpaceRequest* req,
                            ps_rpc::CreateSpaceResponse* resp,
                            ::google::protobuf::Closure* done) {
  auto& local_storager = LocalStorager::GetInstance();
  // TODO: 保存建表语句
  LOG(INFO) << "Query CreateSpace rpc, request:" << req->DebugString();
  RaftManager& raft_mgr = RaftManager::GetInstance();
  common_rpc::RespStatus* status = resp->mutable_status();  // 获取可修改指针
  std::lock_guard<std::mutex> lock(raft_mgr.mtx());
  if (local_storager.PsNodeMata().HasSpace()) {
    std::string msg = "Space has exist, space_name:" +
                      local_storager.PsNodeMata().SpaceReq().space().name();
    LOG(ERROR) << "=============" << msg;
    status->set_code(201);
    status->set_msg(msg);
    done->Run();
    return;
  }

  local_storager.PsNodeMata().SetCreateSapceReq(*req);
  std::string root_path, group, conf;
  int port = 0;
  local_storager.GetRaftParams(root_path, group, conf, port);
  auto msg = Engine::GetInstance().CreatOrLoad(root_path);
  if (msg.size() > 0) {
    status->set_code(201);
    status->set_msg(msg);
    local_storager.PsNodeMata().SetCreateSapceReq(ps_rpc::CreateSpaceRequest());
    done->Run();
    return;
  }
  raft_mgr.CreateBlock();
  msg = raft_mgr.StartRaftServer(root_path, group, conf, port);
  status->set_code(0);
  status->set_msg("");
  if (msg.size() > 0) {
    status->set_code(202);
    status->set_msg(msg);
    local_storager.PsNodeMata().SetCreateSapceReq(ps_rpc::CreateSpaceRequest());
    done->Run();
    return;
  }
  msg = local_storager.PsNodeMata().DumpCreateSpaceReq();
  if (msg.size() > 0) {
    status->set_code(203);
    status->set_msg(msg);
    local_storager.PsNodeMata().SetCreateSapceReq(ps_rpc::CreateSpaceRequest());
  }
  done->Run();
}

void PsRpcImpl::DeleteSpace(::google::protobuf::RpcController* controller,
                            const common_rpc::DeleteSpaceRequest* req,
                            common_rpc::DeleteSpaceResponse* resp,
                            ::google::protobuf::Closure* done) {
  // TODO: 保存建表语句
  LOG(WARNING) << "Query DeleteSpace rpc, request:" << req->DebugString();
  RaftManager& raft_mgr = RaftManager::GetInstance();
  auto& local_storager = LocalStorager::GetInstance();
  auto status = resp->mutable_status();
  std::lock_guard<std::mutex> lock(raft_mgr.mtx());
  if (local_storager.PsNodeMata().DbName() != req->db_name() ||
      local_storager.PsNodeMata().SpaceName() != req->space_name()) {
    LOG(WARNING) << "request db_name:" << local_storager.PsNodeMata().DbName()
                 << " space_name:" << local_storager.PsNodeMata().SpaceName()
                 << " is not exist. delete failed.";
    status->set_code(201);
    status->set_msg("db_name and space_name is not exist.");
    done->Run();
  }
  raft_mgr.DeleteBlock();
  Engine::GetInstance().Delete();
  status->set_code(0);
  status->set_msg("");
  done->Run();
}

void PsRpcImpl::Get(::google::protobuf::RpcController* controller,
                    const ::common_rpc::GetRequest* req,
                    ::common_rpc::GetResponse* resp,
                    ::google::protobuf::Closure* done) {
  done->Run();
}

void PsRpcImpl::Search(::google::protobuf::RpcController* controller,
                       const ::common_rpc::SearchRequest* req,
                       ::common_rpc::SearchResponse* resp,
                       ::google::protobuf::Closure* done) {
  done->Run();
}

// void TestDone::Run() {
//   std::cout << "TestDone::Run()";
// }
