

#include "service/router/router_rpc.h"

#include <brpc/controller.h>  // brpc::Controller
#include <brpc/server.h>      // brpc::Server

#include "common/common.h"
#include "config/conf.h"
#include "service/router/space_manager.h"

SpaceManager& space_manager = SpaceManager::GetInstance();

void RouterSpaceRpcImpl::AddSpace(::google::protobuf::RpcController* controller,
                                  const router_rpc::AddSpaceRequest* req,
                                  router_rpc::AddSpaceResponse* resp,
                                  ::google::protobuf::Closure* done) {
  LOG(INFO) << "====AddSpace api req:" << req->space().DebugString() << "====";
  std::string msg = space_manager.AddSpace(req->space());
  auto status = resp->mutable_status();
  if (msg.size() > 0) {
    status->set_code(101);
    status->set_msg(msg);
  } else {
    LOG(INFO) << "=========AddSpace success, space_name:" << req->space().name()
              << "=======";
    status->set_code(0);
    status->set_msg("");
  }
  done->Run();
}

void RouterSpaceRpcImpl::DeleteSpace(
    ::google::protobuf::RpcController* controller,
    const common_rpc::DeleteSpaceRequest* req,
    common_rpc::DeleteSpaceResponse* resp, ::google::protobuf::Closure* done) {
  LOG(WARNING) << "====DeleteSpace api req:" << req->DebugString() << "====";
  std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  LocalSpaces::GetInstance().DeleteSpace(req->db_name(), req->space_name());
  LocalSpaces::GetInstance().Save();
  std::string msg = space_manager.DeleteSpace(space_key);
  auto status = resp->mutable_status();
  if (msg.size() > 0) {
    status->set_code(101);
    status->set_msg(msg);
    LOG(WARNING) << "DeleteSpace[" << req->space_name() << "] failed";
  } else {
    status->set_code(0);
    status->set_msg("success");
    LOG(WARNING) << "DeleteSpace[" << req->space_name() << "] success:";
  }
  done->Run();
}

void RouterSpaceRpcImpl::SpaceList(
    ::google::protobuf::RpcController* controller,
    const ::google::protobuf::Empty* req, router_rpc::LocalSpaces* resp,
    ::google::protobuf::Closure* done) {
  LOG(INFO) << "=============== /space/list api ==================";
  done->Run();
}

void RouterDocRpcImpl::Get(::google::protobuf::RpcController* controller,
                           const common_rpc::GetRequest* req,
                           common_rpc::GetResponse* resp,
                           ::google::protobuf::Closure* done) {
  LOG(INFO) << "=============== /document/get api =================";

  done->Run();
}

void RouterDocRpcImpl::Search(::google::protobuf::RpcController* controller,
                              const common_rpc::SearchRequest* req,
                              common_rpc::SearchResponse* resp,
                              ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /document/search api ================";

  done->Run();
}

void RouterDocRpcImpl::Upsert(::google::protobuf::RpcController* controller,
                              const common_rpc::UpsertRequest* req,
                              common_rpc::UpsertResponse* resp,
                              ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /document/upsert api ================";

  done->Run();
}

void RouterDocRpcImpl::Delete(::google::protobuf::RpcController* controller,
                              const common_rpc::DeleteRequest* req,
                              common_rpc::DeleteResponse* resp,
                              ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /document/delete api ================";

  done->Run();
}

void RouterDocRpcImpl::MockUpsert(::google::protobuf::RpcController* controller,
                                  const common_rpc::MockUpsertRequest* req,
                                  common_rpc::MockUpsertResponse* resp,
                                  ::google::protobuf::Closure* done) {
  LOG(INFO) << "========== /document/mock/upsert api db_name:["
            << req->db_name() << "] space_name:[" << req->space_name()
            << "]========";
  std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  std::string msg, data;

  const auto space_conn = space_manager.GetSpaceConn(space_key, msg);
  if (msg.size() > 0 || space_conn == nullptr) {
    LOG(ERROR) << "/document/mock/upsert api failed, space_key:" << space_key
               << " not exist";
    resp->set_code(101);
    resp->set_msg(msg);
    done->Run();
    return;
  }
  int partition_num = space_conn->PartitionNum();
  auto docs_grps = space_manager.SplitDocs(req->docs(), partition_num);

  std::cout << "****************router api test_id:" << req->test_id() << "\n";

  auto status_mp =
      space_manager.UpsertDocsToPs(space_key, docs_grps, msg, req->test_id());
  for (const auto& req_doc : req->docs()) {
    auto doc = resp->mutable_docs_res()->Add();
    doc->set_key(req_doc.key());
    doc->set_msg(status_mp[req_doc.key()]);
  }
  resp->set_msg(msg);
  if (msg.size() > 0) {
    resp->set_code(101);
  } else {
    resp->set_code(0);
    LOG(INFO) << "=========MockUpsert success, space_key:" << space_key
              << "=======";
  }
  done->Run();
}

void RouterDocRpcImpl::MockGet(::google::protobuf::RpcController* controller,
                               const common_rpc::MockGetRequest* req,
                               common_rpc::MockGetResponse* resp,
                               ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /document/mock/get api db_name:["
            << req->db_name() << "] space_name:[" << req->space_name()
            << "]========";

  std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  std::string msg, data;

  const auto space_conn = space_manager.GetSpaceConn(space_key, msg);
  if (msg.size() > 0 || space_conn == nullptr) {
    LOG(ERROR) << "/document/mock/get api failed, space_key:" << space_key
               << " not exist";
    resp->set_code(101);
    resp->set_msg(msg);
    done->Run();
    return;
  }
  std::vector<std::string> keys;
  for (const auto& key : req->keys()) {
    keys.push_back(key);
  }
  auto docs_res = space_manager.GetDocsFromPs(space_key, keys, msg);
  resp->set_code(0);
  resp->set_msg(msg);
  for (int i = 0; i < keys.size(); ++i) {
    auto resp_doc = resp->add_docs();
    *resp_doc = std::move(docs_res[keys[i]]);
  }
  done->Run();
}
// void TestDone::Run() {
//   std::cout << "TestDone::Run()";
// }
