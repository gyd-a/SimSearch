

#include "service/ps/ps_register.h"

#include <string>

#include "clients/etcd_client.h"
#include "common/common.h"
#include "config/conf.h"
#include "idl/gen_idl/rpc_service_idl/master_rpc.pb.h"
#include "utils/brpc_http.h"
#include "utils/log.h"

int PsRegister::Init(const std::string& ip, int port, int id) {
  _ip = ip;
  _port = port;
  _id = id;
  _node_V = std::to_string(_port);

  // auto [key, val] = GetPsRegisterKV(_ip, _port, _id);
  if (_etcd_cli.Init(toml_conf_.GetMasterIpPorts()) == false) {
    LOG(ERROR) << "****** Etcd client init error, Ps process exit ******";
    return -1;
  }
  return 0;
}

int PsRegister::LaunchRegister(bool has_space) {
  const auto& master_urls = toml_conf_.master_urls;
  auto* args =
      new PsRegister::LaunchRegisterArgs(has_space, _node_V, &_etcd_cli, _node_K);
  args->_req.set_ip(_ip);
  args->_req.set_port(_port);
  args->_req.set_id(_id);
  bthread_start_background(&_tid, nullptr, this->LaunchRegisterImpl, (void**)args);
  if (has_space == false) {
    int64_t ret_code = 0;
    bthread_join(_tid, (void**)(&ret_code));  // 获取返回值
    LOG(INFO) << "register ps node_key:" << _node_K << ", node_key:" << _node_V;
    return ret_code;
  }
  return 0;
}

void* PsRegister::LaunchRegisterImpl(void* launch_args) {
  PsRegister::LaunchRegisterArgs* args =
      static_cast<PsRegister::LaunchRegisterArgs*>(launch_args);
  int64_t res_code = 0;
  const auto& master_ip_ports = toml_conf_.master_urls;
  do {
    LOG(INFO) << "LaunchRegister thread start register, has_space:" << args->_has_space;
    master_rpc::RegisterResponse resp;
    int ok = HttpPost<master_rpc::RegisterRequest, master_rpc::RegisterResponse>(
        master_ip_ports, "/api/register", args->_req, resp, master_ip_ports.size() * 2);
    if (ok == 0 && resp.code() == 0) {
      args->_register_K = resp.register_key();
      LOG(INFO) << "LaunchRegister thread register success.";
      break;
    }
    if (args->_has_space == false) {
      LOG(ERROR) << "Failed to register node with master, code: " << resp.code()
                 << ", msg: " << resp.msg() << ", HttpPost ret:" << ok;
      res_code = -100;
      delete args;
      return (void*)res_code;
    }
    sleep(5);
  } while (args->_has_space);
  res_code = args->_cli->RegisterNodeWithKeepAlive(args->_register_K, args->_val,
                                                   toml_conf_.ps.hearbeat_interval_s);
  LOG(INFO) << "LaunchRegister thread finished !!!";
  delete args;
  return (void*)res_code;
}

PsRegister& PsRegister::GetInstance() {
  static PsRegister instance;
  return instance;
}

