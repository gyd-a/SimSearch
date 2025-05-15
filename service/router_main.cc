#include <brpc/channel.h>
#include <brpc/server.h>
#include <butil/file_util.h>
#include <butil/logging.h>
// #include <glog/logging.h>
#include <gflags/gflags.h>

#include <memory>
#include <sstream>
#include <thread>

#include "clients/etcd_client.h"
#include "config/conf.h"
#include "google/protobuf/util/json_util.h"
#include "service/router/local_spaces.h"
#include "service/router/router_server.h"
#include "service/router/space_manager.h"
#include "utils/net.h"


DEFINE_string(ip, "", "local ip");
DEFINE_string(port, "", "running port");
DEFINE_string(conf, "config.toml", "conf path");


std::string GenRouterNodeKey(const std::string& node_IP, int port) {
  std::ostringstream node_key_oss;
  node_key_oss << _router_register_prefix << ":" << node_IP << ":" << port;
  return node_key_oss.str();
};


int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_log_dir = "./logs";
  FLAGS_logtostderr = false;  // false: 日志写入到日志， true: 日志输出到标准错误
#ifdef DEBUG_BUILD // true: 日志写入日志同时也输出到标准错误， false： 只写入到日志
  FLAGS_alsologtostderr = true;
#else
  FLAGS_alsologtostderr = false;
#endif

  butil::CreateDirectory(butil::FilePath(FLAGS_log_dir));
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "********************* start Router **********************";
  InitConfig(FLAGS_ip, "", FLAGS_port, FLAGS_conf);

  LocalSpaces::GetInstance().Init(_router_store_path, "local_spaces.json");

  std::string router_node_key = GenRouterNodeKey(_cur_IP, toml_conf_.router.port);
  std::shared_ptr<brpc::EtcdClient> etcd_cli(new brpc::EtcdClient());
  LOG(INFO) << "router_node_key:" << router_node_key;
  if (etcd_cli->Init(toml_conf_.GetMasterIpPorts()) == false) {
    LOG(ERROR) << "****** Etcd client init error, Router process exit ******";
    return -1;
  }
  std::shared_ptr<RouterServer> router_sv(new RouterServer());
  if (router_sv->Start() != 0) {
    LOG(ERROR) << "****** start brpc error, Router process exit ******";
    return -1;
  }

  if (SpaceManager::GetInstance().Init(1000, etcd_cli).size() > 0) {
    LOG(ERROR) << "***** Failed to init SpaceManager ******";
    return -1;
  }

  etcd_cli->RegisterNodeWithKeepAlive(router_node_key, "test_value",
                                      toml_conf_.router.hearbeat_interval_s);

  router_sv->WaitBrpcStop();
  return 0;
}
