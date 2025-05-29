#include "service/ps/local_storager.h"

#include <brpc/channel.h>
#include <config/conf.h>
#include <json2pb/json_to_pb.h>

#include <limits>
#include <random>
#include <sstream>

#include "idl/gen_idl/rpc_service_idl/master_rpc.pb.h"
#include "json2pb/pb_to_json.h"
#include "service/ps/local_mata.h"
#include "common/common.h"

// 生成一个 >= 0 的随机整数，默认范围 [0, INT_MAX]
int GenerateNonNegativeRandomInt(int min = 0,
                                 int max = std::numeric_limits<int>::max()) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  // 确保最小值不小于0
  min = std::max(min, 0);
  std::uniform_int_distribution<> distrib(min, max);
  return distrib(gen);
}

LocalStorager::LocalStorager() {}

LocalStorager::~LocalStorager() {}

int64_t LocalStorager::GetNodeIdByQueryApi(int max_retry) {
  const auto& master_urls = toml_conf_.master_urls;
  for (size_t i = 0; i < max_retry; ++i) {
    int addr_idx = GenerateNonNegativeRandomInt(0, master_urls.size() - 1);
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.protocol = "http";
    options.timeout_ms = 1000;  // 请求超时 ms
    options.connect_timeout_ms = 200;
    options.max_retry = 1;

    if (channel.Init(master_urls[addr_idx].c_str(), "", &options) != 0) {
      LOG(ERROR) << "Fail to initialize channel";
      return -1;
    }

    // 构造 controller
    brpc::Controller cntl;
    // 设置 HTTP URI 和 method
    cntl.http_request().uri() = "/api/gen_id";
    cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
    cntl.http_request().SetHeader("Content-Type", "application/json");

    master_rpc::MasterGenIdRequest request;
    request.set_id(-1);
    // 构造请求 body（json），可以使用 json2pb 也可以手动写
    std::string json_body;
    json2pb::Pb2JsonOptions req_options;
    req_options.bytes_to_base64 = true;  // 若你的 pb 有 bytes 字段建议开启
    json2pb::ProtoMessageToJson(request, &json_body, req_options);
    cntl.request_attachment().append(json_body);
    // 发起请求
    channel.CallMethod(NULL, &cntl, NULL, NULL, NULL);
    if (cntl.Failed()) {
      LOG(ERROR) << "query Mastr gen_id HTTP api request failed: "
                 << cntl.ErrorText() << " request:" << json_body;
    } else {
      std::cout << "HTTP response code: " << cntl.http_response().status_code()
                << std::endl;
      std::string resp_str = cntl.response_attachment().to_string();
      master_rpc::MasterGenIdResponse resp;
      json2pb::Json2PbOptions resp_options;
      resp_options.base64_to_bytes = true;  // proto有bytes类型字段建议打开
      if (json2pb::JsonToProtoMessage(resp_str, &resp, resp_options)) {
        if (resp.id() >= 0 && resp.msg() == "success") {
          LOG(INFO) << "Get node id: " << resp.id()
                    << ", from response_str: " << resp_str;
          return resp.id();
        } else {
          LOG(ERROR) << "Failed to get node id from response: " << resp.msg();
        }
      }
    }
  }
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
    int64_t node_id = GetNodeIdByQueryApi(3);
    if (node_id >= 0) {
      _ps_node_mata.SetPsId(node_id);
      if (_ps_node_mata.DumpNodeInfo().size() == 0) {
        return node_id;
      }
    }
    // TODO: 当前节点和load的节点不一致的处理逻辑

    return -1;
  }
  return _ps_node_mata.PsId();
}

std::string LocalStorager::DeleteSpace(const std::string& db_name, const std::string& space_name) {
  if (db_name == _ps_node_mata.DbName() && space_name == _ps_node_mata.SpaceName()) {
    _ps_node_mata.DeleteSpace();
    return "";
  }
  LOG(ERROR) << "LocalStorager delete space failed, query db_name:" << db_name 
             << ", space_name:" << space_name << " unequal to Ps db_name:" 
             << _ps_node_mata.DbName() << ", space_name:" << _ps_node_mata.SpaceName();
  return "Incorrect db_name or space_name";
}


std::pair<std::string, std::string> LocalStorager::GenPsNodeKeyAndValue() {
  std::ostringstream node_key_oss;
  node_key_oss << _ps_register_prefix << ":" << _ps_node_mata.PsId() << ":"
               << _ps_node_mata.PsIP() << ":" << _ps_node_mata.PsPort();

  return {node_key_oss.str(), std::to_string(_ps_node_mata.PsPort())};
}

std::string LocalStorager::GetRaftRoot() {
  if (_ps_node_mata.HasSpace() == false) { return ""; }
  const auto& cur_partition = _ps_node_mata.SpaceReq().cur_partition();
  return _ps_raft_root_path + "/" + cur_partition.group_name();
}

bool LocalStorager::GetRaftParams(std::string& root_path, std::string& group, 
                                  std::string& conf, int& port) {
  if (_ps_node_mata.HasSpace() == false) { return false; }
  const auto& cur_partition = _ps_node_mata.SpaceReq().cur_partition();
  group = cur_partition.group_name();
  // root_path = _ps_raft_root_path + "/" + group;
  root_path = GetRaftRoot();
  conf = GetRaftConf(cur_partition);
  port = toml_conf_.ps.rpc_port;
  return true;
}

LocalStorager& LocalStorager::GetInstance() {
  static LocalStorager instance = LocalStorager();
  return instance;
}
