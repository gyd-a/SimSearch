
#include "service/router/local_spaces.h"

#include <butil/file_util.h>

#include <fstream>
#include <string>
#include <vector>

#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include "idl/gen_idl/rpc_service_idl/router_rpc.pb.h"
#include "json2pb/json_to_pb.h"
#include "json2pb/pb_to_json.h"

bool LocalSpaces::Init(const std::string& path, const std::string& filename) {
  _path = path;
  _filename = filename;
  butil::FilePath file_path(path);
  if (!butil::PathExists(file_path)) {  // 不存在文件
    if (!butil::CreateDirectory(file_path)) {
      LOG(ERROR) << "Router failed to create LocalSpaces store dir:" << path;
      return false;
    }
  } else {
    for (int i = 0; i < 100; ++i) {
      std::string path_file = _path + "/" + _filename + std::to_string(i);
      butil::FilePath fp(path_file);
      if (butil::PathExists(fp) && !butil::DirectoryExists(fp)) {
      } else {
        butil::CopyFile(file_path, fp);
        break;
      }
    }
  }
  return true;
}

bool LocalSpaces::Load() {
  _pb_spaces.Clear();
  std::string path_file = _path + "/" + _filename;
  std::ifstream ifs(path_file);
  if (!ifs.is_open()) {
    LOG(WARNING) << "Router LocalSpaces not exits. path_file:" << path_file;
    return false;
  }
  std::string json_content((std::istreambuf_iterator<char>(ifs)),
                           std::istreambuf_iterator<char>());
  json2pb::Json2PbOptions options;
  options.base64_to_bytes = true;  // proto有bytes类型字段建议打开
  if (!json2pb::JsonToProtoMessage(json_content, &_pb_spaces, options)) {
    LOG(ERROR) << "Router failed to parse LocalSpaces JSON to protobuf. "
                  "path_file:"
               << path_file;
    return false;
  }
  return true;
}

bool LocalSpaces::Save() {
  LOG(WARNING) << "Save Router Local Space";
  std::string path_file = _path + "/" + _filename;
  std::ofstream ofs(path_file);
  if (!ofs.is_open()) {
    LOG(ERROR) << "Router failed to open LocalSpaces file. path_file:"
               << path_file;
    return false;
  }
  json2pb::Pb2JsonOptions options;
  options.bytes_to_base64 = true;  // proto有bytes类型字段建议打开
  std::string json_str;
  if (!json2pb::ProtoMessageToJson(_pb_spaces, &json_str, options)) {
    LOG(ERROR) << "Router failed to convert protobuf to JSON. json_str:"
               << json_str;
    return false;
  }
  ofs << json_str;
  ofs.close();
  return true;
}

void LocalSpaces::SetSpaces(const std::vector<common_rpc::Space>& spaces) {
  _pb_spaces.Clear();
  for (const auto& space : spaces) {
    AddSpace(space);
  }
}

bool LocalSpaces::AddSpace(const common_rpc::Space& space) {
  common_rpc::Space* new_space = _pb_spaces.add_spaces();
  new_space->CopyFrom(space);
  return true;
}

bool LocalSpaces::DeleteSpace(const std::string& db_name,
                              const std::string& space_name) {
  auto* spaces = _pb_spaces.mutable_spaces();
  LOG(WARNING) << "Delete Router Local Space";
  bool is_del = false;
  // 使用下标倒序遍历删除，避免元素移动影响后续下标
  for (int i = spaces->size() - 1; i >= 0; --i) {
    const auto& space = spaces->Get(i);
    if (space.db_name() == db_name && space.name() == space_name) {
      spaces->DeleteSubrange(i, 1);  // 删除第 i 个元素
      is_del = true;
    }
  }
  return is_del;
}

std::vector<common_rpc::Space> LocalSpaces::GetSpaces() {
  std::vector<common_rpc::Space> spaces;
  for (int i = 0; i < _pb_spaces.spaces_size(); ++i) {
    common_rpc::Space space;
    space.CopyFrom(_pb_spaces.spaces(i));
    spaces.emplace_back(std::move(space));
  }
  return spaces;
}

LocalSpaces& LocalSpaces::GetInstance() {
  static LocalSpaces instance;
  return instance;
}
