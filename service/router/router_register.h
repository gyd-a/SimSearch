#pragma once

#include <string>

#include "clients/etcd_client.h"
#include "string"

class RouterRegister {
 public:
  int Init(const std::string& ip, int port);

  brpc::EtcdClient* EtcdCli() {
    return &_etcd_cli;
  }

  void LaunchRegister();

  static RouterRegister& GetInstance();

 private:
  brpc::EtcdClient _etcd_cli;
  std::string _ip;
  int _port;
  std::string _node_K;
  std::string _node_V;

  RouterRegister() {};
  RouterRegister(const RouterRegister&) = delete;
  RouterRegister(RouterRegister&&) = delete;
  RouterRegister& operator=(const RouterRegister&) = delete;
  RouterRegister& operator=(RouterRegister&&) = delete;
};
