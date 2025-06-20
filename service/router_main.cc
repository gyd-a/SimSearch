#include <brpc/channel.h>
#include <brpc/server.h>
#include <butil/file_util.h>
#include <butil/logging.h>
// #include <glog/logging.h>
#include <gflags/gflags.h>

#include <memory>
#include <sstream>
#include <thread>

#include <unistd.h>
#include "clients/etcd_client.h"
#include "config/conf.h"
#include "google/protobuf/util/json_util.h"
#include "service/router/local_spaces.h"
#include "service/router/router_server.h"
#include "service/router/space_manager.h"
#include "utils/net.h"
#include "common/common.h"
#include "service/router/router_register.h"

DEFINE_string(ip, "", "local ip");
DEFINE_string(port, "", "running port");
DEFINE_string(conf, "config.toml", "conf path");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_log_dir = "./logs";
  FLAGS_logtostderr =
      false;  // false: 日志写入到日志， true: 日志输出到标准错误
#ifdef DEBUG_BUILD  // true: 日志写入日志同时也输出到标准错误， false：
                    // 只写入到日志
  FLAGS_alsologtostderr = true;
#else
  FLAGS_alsologtostderr = false;
#endif

  butil::CreateDirectory(butil::FilePath(FLAGS_log_dir));
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "********* start Router, pid:" << getpid() << " *********";
  InitConfig(FLAGS_ip, "", FLAGS_port, FLAGS_conf);

  LocalSpaces::GetInstance().Init(_router_store_path, "local_spaces.json");

  auto& regist = RouterRegister::GetInstance();
  regist.Init(_cur_IP, toml_conf_.router.port);
  std::shared_ptr<RouterServer> router_sv(new RouterServer());
  if (router_sv->Start() != 0) {
    LOG(ERROR) << "****** start brpc error, Router process exit ******";
    return -1;
  }

  if (SpaceManager::GetInstance().Init(1000, regist.EtcdCli()).size() > 0) {
    LOG(ERROR) << "***** Failed to init SpaceManager ******";
    return -1;
  }

  regist.LaunchRegister();
  router_sv->WaitBrpcStop();
  return 0;
}
