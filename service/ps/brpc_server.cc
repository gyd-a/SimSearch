
#include <brpc/server.h>          // brpc::Server

#include "config/conf.h"
#include "service/ps/brpc_server.h"
#include "raft_store/raft_store.h"

std::string BrpcServer::Start() {
  if (_server.AddService(&_ps_impl, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    std::string msg = "Fail to add PsRpcImpl service";
    LOG(ERROR) << msg;
    return msg;
  }

  // Add your service into RPC rerver
  if (_server.AddService(&_raft_impl, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    std::string msg = "Fail to add RaftRpcImpl";
    LOG(ERROR) << msg;
    return msg;
  }

  // raft can share the same RPC server. Notice the second parameter, because
  // adding services into a running server is not allowed and the listen
  // address of this server is impossible to get before the server starts. You
  // have to specify the address of the server.
  if (braft::add_service(&_server, toml_conf_.ps.rpc_port) != 0) {
    std::string msg = "Fail to add raft service";
    LOG(ERROR) << msg;
    return msg;
  }
  // // 开启 HTTP 服务支持
  // brpc::ServerOptions options;
  // options.enable_http_service = true;
  if (_server.Start(toml_conf_.ps.rpc_port, NULL) != 0) {
    std::string msg = "Fail to start PrRpcImpl Server";
    LOG(ERROR) << msg;
    return msg;
  }
  LOG(INFO) << "====Start Brpc server success, addr:"
            << _server.listen_address() << "====";
  return "";
}

void BrpcServer::WaitBrpcStop() {
  // Wait until 'CTRL-C' is pressed. then Stop() and Join() the service
  while (!brpc::IsAskedToQuit()) {
    sleep(1);
  }
  LOG(INFO) << "PsServer service is going to quit.";

  // Stop block before server
  if (RaftManager::GetInstance().block()) {
    RaftManager::GetInstance().block()->shutdown();
    LOG(WARNING) << "------------------- raft block shutdown ------------------";
  }
  _server.Stop(0);
  LOG(WARNING) << "------------------- brpc server stop ------------------";


  // Wait until all the processing tasks are over.
  if (RaftManager::GetInstance().block()) {
    RaftManager::GetInstance().block()->join();
    LOG(WARNING) << "------------------- raft block join ------------------";
  }

  _server.Join();
  LOG(WARNING) << "------------------- brpc server join ------------------";
  LOG(INFO) << "PsServer complete join.";
}
