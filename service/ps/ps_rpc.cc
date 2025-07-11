

#include "service/ps/ps_rpc.h"

#include <braft/raft.h>           // braft::Node braft::StateMachine
#include <braft/util.h>           // braft::AsyncClosureGuard
#include <brpc/controller.h>      // brpc::Controller
#include <brpc/server.h>          // brpc::Server
#include <butil/sys_byteorder.h>  // butil::NetToHost32
#include <fcntl.h>                // open
#include <google/protobuf/util/json_util.h>
#include <sys/types.h>  // O_CREAT

#include <string>

#include "config/conf.h"
#include "engine/engine.h"
#include "raft_store/raft_store.h"
#include "service/ps/local_storager.h"
#include "common/dto.h"

#include "gamma/c_api/api_data/table.h"
#include "gamma/c_api/use_gamma.h"
#include "utils/log.h"


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
  auto& local_store = LocalStorager::GetInstance();
  auto& mata = local_store.PsNodeMata();
  std::string req_json, msg;
  google::protobuf::util::MessageToJsonString(*req, &req_json);
  // TODO: 保存建表语句
  LOG(INFO) << "Query CreateSpace rpc, request:" << req_json;
  RaftManager& raft_mgr = RaftManager::GetInstance();
  common_rpc::RespStatus* status = resp->mutable_status();  // 获取可修改指针
  std::lock_guard<std::mutex> lock(raft_mgr.mtx());
  if (mata.HasSpace()) {
    msg = "Space has exist, space_name:" + mata.SpaceReq().space().space_name();
    LOG(ERROR) << "=============" << msg << "=============";
    status->set_code(201);
    status->set_msg(msg);
    done->Run();
    return;
  }

  mata.SetCreateSapceReq(*req);
  std::string root_path, group, conf;
  int port = 0;
  local_store.GetRaftParams(root_path, group, conf, port);
  // auto msg = Engine::GetInstance().CreatOrLoad(root_path);
  gamma_cpp_api::InitEngine(_engine_data_dir, mata.SpaceReq().space().space_name());
  vearch::TableInfo table_info = GetGammaTable(mata.SpaceReq().space());
  auto ret_status = gamma_cpp_api::CreateTable(table_info);

  if (ret_status.code() != 0) {
    status->set_code(ret_status.code());
    msg = "Engine create table failed, msg:" + ret_status.ToString();
    status->set_msg(msg);
    mata.SetCreateSapceReq(ps_rpc::CreateSpaceRequest());
    done->Run();
    return;
  }
  raft_mgr.CreateBlock();
  msg = raft_mgr.StartRaftServer(root_path, group, conf, mata.IP(), port, req->space());
  status->set_code(0);
  status->set_msg("");
  if (msg.size() > 0) {
    status->set_code(202);
    status->set_msg(msg);
    mata.SetCreateSapceReq(ps_rpc::CreateSpaceRequest());
    done->Run();
    return;
  }
  msg = mata.DumpCreateSpaceReq();
  if (msg.size() > 0) {
    status->set_code(203);
    status->set_msg(msg);
    mata.SetCreateSapceReq(ps_rpc::CreateSpaceRequest());
  }
  done->Run();
}

void PsRpcImpl::DeleteSpace(::google::protobuf::RpcController* controller,
                            const common_rpc::DeleteSpaceRequest* req,
                            common_rpc::DeleteSpaceResponse* resp,
                            ::google::protobuf::Closure* done) {
  std::string req_json;
  google::protobuf::util::MessageToJsonString(*req, &req_json);
  LOG(WARNING) << "Query DeleteSpace rpc, req_json:" << req_json;
  RaftManager& raft_mgr = RaftManager::GetInstance();
  auto& local_store = LocalStorager::GetInstance();
  auto& mata = local_store.PsNodeMata();
  auto status = resp->mutable_status();
  std::lock_guard<std::mutex> lock(raft_mgr.mtx());
  if (mata.DbName() != req->db_name() || mata.SpaceName() != req->space_name()) {
    LOG(WARNING) << "request db_name:" << mata.DbName()
                 << " space_name:" << mata.SpaceName() << " is not exist. delete failed.";
    status->set_code(201);
    status->set_msg("db_name and space_name is not exist.");
    done->Run();
    return;
  }
  raft_mgr.DeleteBlock();
  gamma_cpp_api::Close();
  butil::DeleteFile(butil::FilePath(_engine_data_dir), true);
  local_store.DeleteSpace(req->db_name(), req->space_name());
  status->set_code(0);
  status->set_msg("");
  done->Run();
}

void PsRpcImpl::Get(::google::protobuf::RpcController* controller,
                    const ::common_rpc::GetRequest* req, ::common_rpc::GetResponse* resp,
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
