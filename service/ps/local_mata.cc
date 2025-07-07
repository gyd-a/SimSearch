#include "service/ps/local_mata.h"

#include <butil/file_util.h>
#include <json2pb/json_to_pb.h>

#include <fstream>
#include <string>

#include "json2pb/pb_to_json.h"
#include "utils/log.h"

PsLocalNodeMata::PsLocalNodeMata() {}

PsLocalNodeMata::~PsLocalNodeMata() {}

bool PsLocalNodeMata::Init(const std::string& dir_path) {
  _dir_path = dir_path;
  _node_info_file = _dir_path + "/node_info.json";
  _space_schema_file = dir_path + "/create_space_req.json";
  butil::FilePath file_path(_dir_path);
  if (!butil::PathExists(file_path)) {  // 不存在文件
    if (!butil::CreateDirectory(file_path)) {
      LOG(ERROR) << "Failed to create directory: " << _dir_path;
      return true;
    }
  } else {  // 存在文件
    std::ifstream ifs(_node_info_file);
    if (!ifs.is_open()) {
      LOG(WARNING) << "ps mata dir exit, but JSON file not exits. "
                   << "node_info_file:" << _node_info_file;
      return true;
    }
    std::string json_content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
    json2pb::Json2PbOptions options;
    options.base64_to_bytes = true;  // proto有bytes类型字段建议打开
    if (!json2pb::JsonToProtoMessage(json_content, &_ps_node_info, options)) {
      LOG(ERROR) << "Failed to parse ps mata JSON to protobuf. node_info_file:"
                 << _node_info_file;
      return false;
    }
    LOG(INFO) << "load ps node_id[" << _ps_node_info.ps_id()
              << "] successful from file, node_info_file:" << _node_info_file;
    std::ifstream space_ifs(_space_schema_file);
    if (!space_ifs.is_open()) {
      LOG(INFO) << "not has create_space_req.json file, file_path:" << _space_schema_file;
      return true;
    }
    std::string space_json((std::istreambuf_iterator<char>(space_ifs)),
                           std::istreambuf_iterator<char>());
    if (!json2pb::JsonToProtoMessage(space_json, &_space_req, options)) {
      LOG(ERROR) << "Failed to parse create_space_req.json to protobuf. "
                 << "file_path:" << _space_schema_file;
      return false;
    }
    _has_space = true;
  }
  return true;
}

bool PsLocalNodeMata::IsLoadFromFile() {
  if (_ps_node_info.ps_id() > 0 && _ps_node_info.ps_ip().length() > 0) {
    return true;
  }
  return false;
}

std::string PsLocalNodeMata::DumpCreateSpaceReq() {
  std::string json_str, msg;
  json2pb::Pb2JsonOptions options;
  options.bytes_to_base64 = true;  // 若你的 pb 有 bytes 字段建议开启
  if (_space_req.space().db_name().size() > 0) {
    if (!json2pb::ProtoMessageToJson(_space_req, &json_str, options)) {
      msg = "Failed to convert protobuf to JSON. json_str:" + json_str;
      LOG(ERROR) << msg;
      return msg;
    }
    std::ofstream space_ofs(_space_schema_file);
    if (!space_ofs.is_open()) {
      msg = "Failed to open create_space_req: " + _space_schema_file +
            " json_str:" + json_str;
      LOG(ERROR) << msg;
      return msg;
    }
    space_ofs << json_str;
    space_ofs.close();
    return msg;
  }
  msg = "space is not set. not dump CreateSpaceReq";
  LOG(ERROR) << msg;
  return msg;
}

std::string PsLocalNodeMata::DumpNodeInfo() {
  std::string json_str, msg;
  json2pb::Pb2JsonOptions options;
  options.bytes_to_base64 = true;  // 若你的 pb 有 bytes 字段建议开启
  if (_ps_node_info.ps_id() <= 0) {
    msg = "_ps_node_info is not null or id is not set. skip";
    LOG(ERROR) << msg;
    return msg;
  }
  json_str.clear();
  if (!json2pb::ProtoMessageToJson(_ps_node_info, &json_str, options)) {
    msg = "Failed to convert protobuf to JSON. json_str:" + json_str;
    LOG(ERROR) << msg;
    return msg;
  }
  std::ofstream ofs(_node_info_file);
  if (!ofs.is_open()) {
    msg = "Failed to open file, node_info_file: " + _node_info_file +
          " json_str:" + json_str;
    LOG(ERROR) << msg;
    return msg;
  }
  ofs << json_str;
  ofs.close();
  return msg;
}

bool PsLocalNodeMata::SetCreateSapceReq(const ps_rpc::CreateSpaceRequest& space_req) {
  _space_req = space_req;
  if (_space_req.space().db_name().size() > 0) {
    _has_space = true;
  }
  return true;
}

std::string PsLocalNodeMata::SpaceKey() {
  return GenSpaceKey(_space_req.space().db_name(), _space_req.space().space_name());
}

bool PsLocalNodeMata::SetPsId(int64_t ps_node_id) {
  _ps_node_info.set_ps_id(ps_node_id);
  return true;
}

bool PsLocalNodeMata::SetPsPort(int32_t raft_port) {
  _ps_node_info.set_ps_port(raft_port);
  return true;
}

bool PsLocalNodeMata::SetPsIP(const std::string& ps_node_IP) {
  _ps_node_info.set_ps_ip(ps_node_IP);
  return true;
}

int64_t PsLocalNodeMata::Id() {
  if (_ps_node_info.ps_id() <= 0) {
    LOG(ERROR) << "GetPsNodeId() Failed. ps_node_info is null";
  }
  return _ps_node_info.ps_id();
}

const std::string& PsLocalNodeMata::IP() { return _ps_node_info.ps_ip(); }

int32_t PsLocalNodeMata::Port() { return _ps_node_info.ps_port(); }

const ps_rpc::CreateSpaceRequest& PsLocalNodeMata::SpaceReq() { return _space_req; }

bool PsLocalNodeMata::HasSpace() { return _has_space.load() == true ? true : false; }

std::string PsLocalNodeMata::DbName() { return _space_req.space().db_name(); };

std::string PsLocalNodeMata::SpaceName() { return _space_req.space().space_name(); };

std::string PsLocalNodeMata::DeleteSpace() {
  LOG(WARNING) << "PsLocalNodeMata delete space:" << _space_req.space().space_name();
  _space_req.Clear();
  butil::DeleteFile(butil::FilePath(_space_schema_file), false);
  _has_space = false;
  return "";
}

std::string PsLocalNodeMata::DeleteNodeInfo() {
  LOG(WARNING) << "PsLocalNodeMata delete node info, file_path:" << _node_info_file;
  _ps_node_info.Clear();
  butil::DeleteFile(butil::FilePath(_node_info_file), false);
  return "";
}

const common_rpc::Space& PsLocalNodeMata::Space() {
  return _space_req.space();
}
