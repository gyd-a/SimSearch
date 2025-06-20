
#include <butil/logging.h>
#include <gflags/gflags.h>
// #include <glog/logging.h>
#include <unistd.h>

#include <memory>
#include <thread>

#include "butil/base64.h"
#include "clients/etcd_client.h"
#include "common/common.h"
#include "config/conf.h"
#include "engine/engine.h"
#include "google/protobuf/util/json_util.h"
#include "idl/gen_idl/etcd_idl/rpc.pb.h"
#include "raft_store/raft_store.h"
#include "service/ps/brpc_server.h"
#include "service/ps/local_storager.h"
#include "service/ps/ps_register.h"

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

  std::unique_ptr<BrpcServer> brpc_sv = std::make_unique<BrpcServer>();

  if (brpc_sv->Start().size() != 0) {
    LOG(ERROR) << "****** start brpc error, Ps process exit ******";
    return -1;
  }
  auto& mata = local_store.PsNodeMata();
  if (mata.HasSpace()) {
    LOG(INFO) << "local file has space, start load Raft.";
    auto& raft_mgr = RaftManager::GetInstance();
    raft_mgr.CreateBlock();
    std::string root_path, grp, conf;
    int port = 0;
    local_store.GetRaftParams(root_path, grp, conf, port);
    if (raft_mgr.StartRaftServer(root_path, grp, conf, mata.IP(), port).size() > 0) {
      LOG(ERROR) << "restart Raft error, server not write, only read.";
    }
  }

  auto& regist = PsRegister::GetInstance();
  regist.Init(mata.IP(), mata.Port(), mata.Id());
  if (regist.LaunchRegister(mata.HasSpace()) == 0) {
    brpc_sv->WaitBrpcStop();
  } else {
    LOG(ERROR) << "****** register ps node error, Ps process exit ******";
    mata.DeleteNodeInfo();
  }
  return 0;
}
