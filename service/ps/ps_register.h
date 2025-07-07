#pragma once


#include <bthread/bthread.h>

#include <string>

#include "idl/gen_idl/rpc_service_idl/master_rpc.pb.h"
#include "clients/etcd_client.h"
#include "string"

class PsRegister {
 public:
  struct LaunchRegisterArgs {
    bool _has_space;
    std::string _val;
    brpc::EtcdClient* _cli;
    master_rpc::RegisterRequest _req;
    std::string& _register_K;  // 输出结果
    LaunchRegisterArgs(bool has_space, const std::string val, brpc::EtcdClient* cli,
                       std::string& register_key)
        : _has_space(has_space), _val(val), _cli(cli), _register_K(register_key) {}
  };

  int Init(const std::string& ip, int port, int id);

  int LaunchRegister(bool has_space);

  static PsRegister& GetInstance();

 private:
  static void* LaunchRegisterImpl(void* launch_args);

  brpc::EtcdClient _etcd_cli;
  std::string _ip;
  int _port;
  int _id;
  std::string _node_K;
  std::string _node_V;
  bthread_t _tid;

  PsRegister() {};
  PsRegister(const PsRegister&) = delete;
  PsRegister(PsRegister&&) = delete;
  PsRegister& operator=(const PsRegister&) = delete;
  PsRegister& operator=(PsRegister&&) = delete;
};
