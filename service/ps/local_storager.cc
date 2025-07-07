#include "service/ps/local_storager.h"

#include <brpc/channel.h>
#include <config/conf.h>
#include <json2pb/json_to_pb.h>
#include <json2pb/pb_to_json.h>

#include <limits>
#include <random>
#include <sstream>

#include "common/common.h"
#include "idl/gen_idl/rpc_service_idl/master_rpc.pb.h"
#include "service/ps/local_mata.h"
#include "utils/brpc_http.h"
#include "utils/log.h"

LocalStorager::LocalStorager() {}

LocalStorager::~LocalStorager() {}

int64_t LocalStorager::GetNodeIdByQueryApi(int max_retry) {
  master_rpc::MasterGenIdRequest req;
  master_rpc::MasterGenIdResponse resp;
  req.set_id(-1);

  auto ok = HttpPost<master_rpc::MasterGenIdRequest, master_rpc::MasterGenIdResponse>(
      toml_conf_.master_urls, "/api/gen_id", req, resp, max_retry);
  if (ok == 0 && resp.id() >= 0 && resp.code() == 0) {
    return resp.id();
  }
  LOG(ERROR) << "Failed to get node id from Master gen_id API, code: " << resp.code()
             << ", msg: " << resp.msg() << ", HttpPost ret:" << ok;
  return -1;
}

int64_t LocalStorager::CreateOrLoadNodeMata(const std::string& dir_path) {
  if (!_ps_node_mata.Init(dir_path)) {
    LOG(ERROR) << "Failed to initialize PsLocalNodeMata";
    return -1;
  }
  if (_ps_node_mata.IsLoadFromFile() == false) {
    _ps_node_mata.SetPsIP(_cur_IP);
    _ps_node_mata.SetPsPort(toml_conf_.ps.rpc_port);
    int64_t node_id = GetNodeIdByQueryApi(4);
    if (node_id >= 0) {
      _ps_node_mata.SetPsId(node_id);
      if (_ps_node_mata.DumpNodeInfo().size() == 0) {
        return node_id;
      }
    }
    // TODO: 当前节点和load的节点不一致的处理逻辑

    return -1;
  }
  return _ps_node_mata.Id();
}

std::string LocalStorager::DeleteSpace(const std::string& db_name,
                                       const std::string& space_name) {
  if (db_name == _ps_node_mata.DbName() && space_name == _ps_node_mata.SpaceName()) {
    _ps_node_mata.DeleteSpace();
    return "";
  }
  LOG(ERROR) << "LocalStorager delete space failed, query db_name:" << db_name
             << ", space_name:" << space_name
             << " unequal to Ps db_name:" << _ps_node_mata.DbName()
             << ", space_name:" << _ps_node_mata.SpaceName();
  return "Incorrect db_name or space_name";
}

std::string LocalStorager::GetRaftRoot() {
  if (_ps_node_mata.HasSpace() == false) {
    return "";
  }
  const auto& cur_partition = _ps_node_mata.SpaceReq().cur_partition();
  return _raft_root_dir + "/" + cur_partition.group_name();
}

bool LocalStorager::GetRaftParams(std::string& root_path, std::string& group,
                                  std::string& conf, int& port) {
  if (_ps_node_mata.HasSpace() == false) {
    return false;
  }
  const auto& cur_partition = _ps_node_mata.SpaceReq().cur_partition();
  group = cur_partition.group_name();
  // root_path = _raft_root_dir + "/" + group;
  root_path = GetRaftRoot();
  conf = GetRaftConf(cur_partition);
  port = toml_conf_.ps.rpc_port;
  return true;
}

LocalStorager& LocalStorager::GetInstance() {
  static LocalStorager instance = LocalStorager();
  return instance;
}
