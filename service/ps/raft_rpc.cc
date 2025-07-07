

#include "service/ps/raft_rpc.h"

#include <braft/raft.h>           // braft::Node braft::StateMachine
#include <braft/util.h>           // braft::AsyncClosureGuard
#include <brpc/controller.h>      // brpc::Controller
#include <brpc/server.h>          // brpc::Server
#include <butil/sys_byteorder.h>  // butil::NetToHost32
#include <fcntl.h>                // open
#include <sys/types.h>            // O_CREAT

#include "config/conf.h"
#include "raft_store/raft_store.h"
#include "utils/log.h"


void RaftRpcImpl::write(::google::protobuf::RpcController* controller,
                        const raft_rpc::BlockRequest* request,
                        raft_rpc::BlockResponse* response,
                        ::google::protobuf::Closure* done) {
  brpc::Controller* cntl = (brpc::Controller*)controller;
  LOG(INFO) << "============== /PsRaftService/write api ================";
  RaftManager& raft_mgr = RaftManager::GetInstance();
  std::lock_guard<std::mutex> lock(raft_mgr.mtx());
  if (raft_mgr.block() == nullptr) {
    response->set_success(false);
    response->set_msg("不存在这个space, 或者space has error and not run");
    done->Run();
    return;
  }
  
  raft_mgr.block()->write(request, response, &cntl->request_attachment(), done);
/*
  if (response->success() == false) {
    LOG(ERROR) << "========raft write error in engine, restart raft. ===========";
    std::string grp = raft_mgr.block()->group();
    std::string conf = raft_mgr.block()->conf();
    std::string root_path = raft_mgr.block()->root_path();
    int port = raft_mgr.block()->port();
    raft_mgr.CreateBlock();
    std::string msg = raft_mgr.StartRaftServer(root_path, grp, conf, port);
  }
*/
}

void RaftRpcImpl::read(::google::protobuf::RpcController* controller,
                       const raft_rpc::BlockRequest* request,
                       raft_rpc::BlockResponse* response,
                       ::google::protobuf::Closure* done) {
  LOG(INFO) << "============== /PsRaftService/read api ================";
  brpc::Controller* cntl = (brpc::Controller*)controller;
  brpc::ClosureGuard done_guard(done);
  RaftManager& raft_mgr = RaftManager::GetInstance();
  std::lock_guard<std::mutex> lock(raft_mgr.mtx());
  if (raft_mgr.block() == nullptr) {
    response->set_success(false);
    response->set_msg("不存在这个space, 或者space has error and not run");
  }
  return raft_mgr.block()->read(request, response, &cntl->response_attachment());
}

/*
void RaftRpcImpl::write(::google::protobuf::RpcController* controller,
                        const raft_rpc::BlockRequest* request,
                        raft_rpc::BlockResponse* response,
                        ::google::protobuf::Closure* done) {
  brpc::Controller* cntl = (brpc::Controller*)controller;
  return _block->write(request, response, &cntl->request_attachment(), done);
}

void RaftRpcImpl::read(::google::protobuf::RpcController* controller,
                       const raft_rpc::BlockRequest* request,
                       raft_rpc::BlockResponse* response,
                       ::google::protobuf::Closure* done) {
  brpc::Controller* cntl = (brpc::Controller*)controller;
  brpc::ClosureGuard done_guard(done);
  return _block->read(request, response, &cntl->response_attachment());
}

void RaftRpcImpl::InsertData(::google::protobuf::RpcController* controller,
                             const common_rpc::InsertDataRequest* req,
                             common_rpc::InsertDataResponse* resp,
                             ::google::protobuf::Closure* done) {
  done->Run();
}

void RaftRpcImpl::DeleteData(::google::protobuf::RpcController* controller,
                             const common_rpc::DeleteDataRequest* req,
                             common_rpc::DeleteDataResponse* resp,
                             ::google::protobuf::Closure* done) {
  done->Run();
}
*/

void TestDone::Run() { std::cout << "TestDone::Run()"; }
