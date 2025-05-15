#pragma once

#include <butil/file_util.h>

#include <fstream>
#include <string>
#include <vector>

#include "idl/gen_idl/rpc_service_idl/router_rpc.pb.h"
#include "idl/gen_idl/rpc_service_idl/common_rpc.pb.h"
#include "json2pb/json_to_pb.h"
#include "json2pb/pb_to_json.h"


class LocalSpaces {
 public:
  bool Init(const std::string& path, const std::string& filename);

  bool Load();

  bool Save();

  void SetSpaces(const std::vector<common_rpc::Space>& spaces);

  bool AddSpace(const common_rpc::Space& space);

  bool DeleteSpace(const std::string& db_name, const std::string& space_name);

  std::vector<common_rpc::Space> GetSpaces();

  static LocalSpaces& GetInstance();

 private:
  LocalSpaces() {}
  LocalSpaces(const LocalSpaces&) = delete;
  LocalSpaces(LocalSpaces&&) = delete;
  LocalSpaces& operator=(const LocalSpaces&) = delete;
  LocalSpaces& operator=(LocalSpaces&&) = delete;

  std::string _filename;
  std::string _path;
  router_rpc::LocalSpaces _pb_spaces;
};
