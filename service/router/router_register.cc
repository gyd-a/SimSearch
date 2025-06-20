

#include <string>

#include "clients/etcd_client.h"
#include "common/common.h"
#include "config/conf.h"
#include "service/router/router_register.h"


int RouterRegister::Init(const std::string& ip, int port) {
  _ip = ip;
  _port = port;
  _node_V = std::to_string(_port);
  _node_K = GenRouterNodeKey(_ip, _port);

  // auto [key, val] = GetPsRegisterKV(_ip, _port, _id);
  if (_etcd_cli.Init(toml_conf_.GetMasterIpPorts()) == false) {
    LOG(ERROR) << "****** Etcd client init error, Router process exit ******";
    return -1;
  }
  return 0;
}

void RouterRegister::LaunchRegister() {
  _etcd_cli.RegisterNodeWithKeepAlive(_node_K, _node_V,
                                      toml_conf_.router.hearbeat_interval_s);
}

RouterRegister& RouterRegister::GetInstance() {
  static RouterRegister instance;
  return instance;
}
