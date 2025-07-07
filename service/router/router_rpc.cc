

#include "service/router/router_rpc.h"

#include <brpc/controller.h>  // brpc::Controller
#include <brpc/server.h>      // brpc::Server
#include <set>

#include "common/common.h"
#include "config/conf.h"
#include "service/router/space_manager.h"
#include "utils/log.h"
#include "idl/gen_idl/rpc_service_idl/raft_rpc.pb.h"
#include "common/dto.h"


SpaceManager& space_manager = SpaceManager::GetInstance();

void RouterSpaceRpcImpl::AddSpace(::google::protobuf::RpcController* controller,
                                  const router_rpc::AddSpaceRequest* req,
                                  router_rpc::AddSpaceResponse* resp,
                                  ::google::protobuf::Closure* done) {
  const std::string& db_name = req->space().db_name();
  const std::string& space_name = req->space().space_name();
  LOG(INFO) << "====AddSpace api, db_name:" << db_name << ", space_name:" << space_name << "====";
  std::string msg = space_manager.AddSpace(req->space());
  auto status = resp->mutable_status();
  if (msg.size() > 0) {
    status->set_code(101);
    status->set_msg(msg);
  } else {
    LOG(INFO) << "=========AddSpace success, space_name:" << space_name << "=======";
    status->set_code(0);
    status->set_msg("");
  }
  done->Run();
}

void RouterSpaceRpcImpl::DeleteSpace(
    ::google::protobuf::RpcController* controller,
    const common_rpc::DeleteSpaceRequest* req,
    common_rpc::DeleteSpaceResponse* resp, ::google::protobuf::Closure* done) {
  const std::string& db_name = req->db_name();
  const std::string& space_name = req->space_name();
  LOG(WARNING) << "====DeleteSpace api, db_name:" << db_name << ", space_name:" << space_name << "====";
  std::string space_key = GenSpaceKey(db_name, space_name);
  LocalSpaces::GetInstance().DeleteSpace(db_name, space_name);
  LocalSpaces::GetInstance().Save();
  std::string msg = space_manager.DeleteSpace(space_key);
  auto status = resp->mutable_status();
  if (msg.size() > 0) {
    status->set_code(101);
    status->set_msg(msg);
    LOG(WARNING) << "DeleteSpace[" << space_name << "] failed";
  } else {
    status->set_code(0);
    status->set_msg("success");
    LOG(WARNING) << "DeleteSpace[" << space_name << "] success:";
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
  auto ret_fun = [&resp, &done](int32_t code, const std::string& msg) {
    resp->set_code(code);
    resp->set_msg(msg);
    done->Run();
  };
  auto add_same_msg = [&req, &resp](const std::string& msg) {
    for (const auto& key : req->_ids()) {
      auto doc_status = resp->mutable_failed_docs()->Add();
      doc_status->set__id(key);
      doc_status->set_msg(msg);
    }
  };
  std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  std::string msg;
  std::shared_ptr<SpaceConnection> space_conn = space_manager.GetSpaceConn(space_key, msg);
  if (msg.size() > 0 || space_conn == nullptr) {
    add_same_msg(msg);
    LOG(ERROR) << "/document/get api failed, space_key:" << space_key
               << " not exist";
    ret_fun(101, msg);
    return;
  }
  msg = CheckPbIds(req->_ids(), space_conn->pb_space());
  if (msg.size() > 0) {
    add_same_msg(msg);
    LOG(ERROR) << "Get doc failed, space_key:" << space_key << " msg:" << msg;
    ret_fun(102, msg);
    return;
  }
  //TODO: check if the keys are valid and 删除
  auto idx_keys_mp = space_manager.HashKeys(req->_ids(), space_conn->PartitionNum());
  std::map<std::string, common_rpc::Document> docs_res;
  auto err_ids = space_conn->GetDocs(idx_keys_mp, docs_res);
  for (const auto& key : req->_ids()) {
    if (err_ids.count(key) > 0) {
      auto doc_status = resp->mutable_failed_docs()->Add();
      doc_status->set__id(key);
      doc_status->set_msg(err_ids[key]);
    } else if (docs_res.count(key) > 0) {
      auto doc = resp->mutable_documents()->Add();
      *doc = std::move(docs_res[key]);
    } else {
      auto doc_status = resp->mutable_failed_docs()->Add();
      doc_status->set__id(key);
      doc_status->set_msg("get doc fail, engine not has result.");
    }
  }
  if (err_ids.size() > 0) {
    ret_fun(103, "There is some id that failed to get. failed ids :");
    return;
  }
  ret_fun(0, "");
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
  auto ret_fun = [&resp, &done](int32_t code, const std::string& msg) {
    resp->set_code(code);
    resp->set_msg(msg);
    done->Run();
  };
  auto add_same_msg = [&req, &resp](const std::string& msg) {
    for (const auto& each_doc : req->documents()) {
      auto doc_status = resp->mutable_failed_docs()->Add();
      doc_status->set__id(each_doc._id());
      doc_status->set_msg(msg);
    }
  };
  
  LOG(INFO) << "============== /document/upsert api ================";
  std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  std::string msg, data;
  std::shared_ptr<SpaceConnection> space_conn = space_manager.GetSpaceConn(space_key, msg);
  if (msg.size() > 0 || space_conn == nullptr) {
    LOG(ERROR) << "/document/upsert api failed, space_key:" << space_key
               << " not exist";
    add_same_msg(msg);
    ret_fun(101, msg);
    return;
  }
  std::vector<std::string> doc_keys(req->documents_size());
  for (int i = 0; i < req->documents_size(); ++i) {
    msg = CheckPbDoc(req->documents(i), space_conn->pb_space());
    if (msg.size() > 0) {
      LOG(ERROR) << "space_key:" << space_key << " " << msg;
      add_same_msg("has doc format error");
      ret_fun(102, msg);
      return;
    }
  }

  int partition_num = space_conn->PartitionNum();
  auto docs_grps = space_manager.HashDocs(req->documents(), doc_keys, partition_num);

  auto err_mp = space_conn->UpsertDocs(docs_grps, msg, 0);
  
  for (const auto& doc_key : doc_keys) {
    if (err_mp.count(doc_key)) {
      auto doc_status = resp->mutable_failed_docs()->Add();
      // TODO:
      doc_status->set__id(doc_key);
      doc_status->set_msg(err_mp[doc_key]);
    }
  }
  if (doc_keys.size() > 0 ) {
    ret_fun(103, "upsert exist fail, failed id:");
    return;
  }
  resp->set_code(0);
  LOG(INFO) << "=========Upsert success, space_key:" << space_key << "=======";
  done->Run();
}

void RouterDocRpcImpl::Delete(::google::protobuf::RpcController* controller,
                              const common_rpc::DeleteRequest* req,
                              common_rpc::DeleteResponse* resp,
                              ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /document/delete api ================";
  auto ret_fun = [&resp, &done](int32_t code, const std::string& msg) {
    resp->set_code(code);
    resp->set_msg(msg);
    done->Run();
  };
  auto add_same_msg = [&req, &resp](const std::string& msg) {
    for (const auto& key : req->_ids()) {
      auto doc_status = resp->mutable_failed_docs()->Add();
      doc_status->set__id(key);
      doc_status->set_msg(msg);
    }
  };
  std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  std::string msg;
  std::shared_ptr<SpaceConnection> space_conn = space_manager.GetSpaceConn(space_key, msg);
  if (msg.size() > 0 || space_conn == nullptr) {
    add_same_msg(msg);
    LOG(ERROR) << "/document/delete api failed, space_key:" << space_key
               << " not exist";
    ret_fun(101, msg);
    return;
  }
  msg = CheckPbIds(req->_ids(), space_conn->pb_space());
  if (msg.size() > 0) {
    add_same_msg(msg);
    LOG(ERROR) << "Delete doc failed, space_key:" << space_key << " msg:" << msg;
    ret_fun(102, msg);
    return;
  }
  //TODO: check if the keys are valid and 删除
  auto idx_keys_mp = space_manager.HashKeys(req->_ids(), space_conn->PartitionNum());
  auto err_ids = space_conn->DeleteDocs(idx_keys_mp, msg);
  for (const auto& key : req->_ids()) {
    if (err_ids.count(key) > 0) {
      auto doc_status = resp->mutable_failed_docs()->Add();
      doc_status->set__id(key);
      doc_status->set_msg(err_ids[key]);
    }
  }
  if (err_ids.size() > 0) {
    ret_fun(103, "There is some id that failed to be deleted. failed ids :");
    return;
  }
  ret_fun(0, "");
}

void RouterDocRpcImpl::MockUpsert(::google::protobuf::RpcController* controller,
                                  const common_rpc::MockUpsertRequest* req,
                                  common_rpc::MockUpsertResponse* resp,
                                  ::google::protobuf::Closure* done) {
  // LOG(INFO) << "========== /document/mock/upsert api db_name:["
  //           << req->db_name() << "] space_name:[" << req->space_name()
  //           << "]========";
  // std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  // std::string msg, data;

  // const auto space_conn = space_manager.GetSpaceConn(space_key, msg);
  // if (msg.size() > 0 || space_conn == nullptr) {
  //   LOG(ERROR) << "/document/mock/upsert api failed, space_key:" << space_key
  //              << " not exist";
  //   resp->set_code(101);
  //   resp->set_msg(msg);
  //   done->Run();
  //   return;
  // }
  // int partition_num = space_conn->PartitionNum();
  // auto docs_grps = space_manager.HashDocs(req->docs(), partition_num);

  // std::cout << "****************router api test_id:" << req->test_id() << "\n";

  // auto status_mp =
  //     space_manager.UpsertDocsToPs(space_key, docs_grps, msg, req->test_id());
  // for (const auto& req_doc : req->docs()) {
  //   auto doc = resp->mutable_docs_res()->Add();
  //   doc->set_key(req_doc.key());
  //   doc->set_msg(status_mp[req_doc.key()]);
  // }
  // resp->set_msg(msg);
  // if (msg.size() > 0) {
  //   resp->set_code(101);
  // } else {
  //   resp->set_code(0);
  //   LOG(INFO) << "=========MockUpsert success, space_key:" << space_key
  //             << "=======";
  // }
  // done->Run();
}

void RouterDocRpcImpl::MockGet(::google::protobuf::RpcController* controller,
                               const common_rpc::MockGetRequest* req,
                               common_rpc::MockGetResponse* resp,
                               ::google::protobuf::Closure* done) {
  // LOG(INFO) << "============== /document/mock/get api db_name:["
  //           << req->db_name() << "] space_name:[" << req->space_name()
  //           << "]========";

  // std::string space_key = GenSpaceKey(req->db_name(), req->space_name());
  // std::string msg, data;

  // const auto space_conn = space_manager.GetSpaceConn(space_key, msg);
  // if (msg.size() > 0 || space_conn == nullptr) {
  //   LOG(ERROR) << "/document/mock/get api failed, space_key:" << space_key
  //              << " not exist";
  //   resp->set_code(101);
  //   resp->set_msg(msg);
  //   done->Run();
  //   return;
  // }
  // std::vector<std::string> keys;
  // for (const auto& key : req->keys()) {
  //   keys.push_back(key);
  // }
  // auto docs_res = space_manager.GetDocsFromPs(space_key, keys, msg);
  // resp->set_code(0);
  // resp->set_msg(msg);
  // for (int i = 0; i < keys.size(); ++i) {
  //   auto resp_doc = resp->add_docs();
  //   *resp_doc = std::move(docs_res[keys[i]]);
  // }
  // done->Run();
}
// void TestDone::Run() {
//   std::cout << "TestDone::Run()";
// }
