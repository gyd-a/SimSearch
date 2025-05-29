
#include <butil/logging.h>
#include <gflags/gflags.h>
// #include <glog/logging.h>
#include <memory>
#include <thread>

#include <unistd.h>
#include "butil/base64.h"
#include "clients/etcd_client.h"
#include "config/conf.h"
#include "engine/engine.h"
#include "google/protobuf/util/json_util.h"
#include "idl/gen_idl/etcd_idl/rpc.pb.h"
#include "raft_store/raft_store.h"
#include "service/ps/brpc_server.h"
#include "service/ps/local_storager.h"

DEFINE_string(ip, "", "IP of etcd server");
DEFINE_string(port, "", "port of etcd server");
DEFINE_string(conf, "config.toml", "conf path");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_log_dir = "./logs";
  FLAGS_logtostderr = false;
#ifdef DEBUG_BUILD  // true: 日志写入日志同时也输出到标准错误， false：
                    // 只写入到日志
  FLAGS_alsologtostderr = true;
#else
  FLAGS_alsologtostderr = false;
#endif
  butil::CreateDirectory(butil::FilePath(FLAGS_log_dir));
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "************* start PS, pid:" << getpid() << " ************";
  InitConfig(FLAGS_ip, FLAGS_port, "", FLAGS_conf);
  LocalStorager& local_store = LocalStorager::GetInstance();
  if (local_store.CreateOrLoadNodeMata(_ps_storage_dir) <= 0) {
    LOG(ERROR) << "Failed to CreateOrLoadNodeMata. process exit()";
    return -1;
  }
  Engine::GetInstance().Init(_ps_storage_dir);

  BrpcServer* brpc_sv = new BrpcServer();

  auto[node_K, node_V] = local_store.GenPsNodeKeyAndValue();
  brpc::EtcdClient etcd_cli;
  LOG(INFO) << "ps_node_key:" << node_K << ", value:" << node_V;
  if (etcd_cli.Init(toml_conf_.GetMasterIpPorts()) == false) {
    LOG(ERROR) << "****** Etcd client init error, Ps process exit ******";
    return -1;
  }
  if (brpc_sv->Start().size() != 0) {
    LOG(ERROR) << "****** start brpc error, Ps process exit ******";
    return -1;
  }

  if (local_store.PsNodeMata().HasSpace()) {
    LOG(INFO) << "local file has space, start load Raft.";
    RaftManager::GetInstance().CreateBlock();
    std::string root_path, grp, conf;
    int port = 0;
    local_store.GetRaftParams(root_path, grp, conf, port);
    if (RaftManager::GetInstance()
            .StartRaftServer(root_path, grp, conf, local_store.PsNodeMata().PsIP(), port)
            .size() > 0) {
      LOG(ERROR) << "restart Raft error, server not write, only read.";
    }
  }

  etcd_cli.RegisterNodeWithKeepAlive(node_K, node_V,
                                     toml_conf_.ps.hearbeat_interval_s);
  brpc_sv->WaitBrpcStop();
  delete brpc_sv;
  brpc_sv = nullptr;
  return 0;
}
