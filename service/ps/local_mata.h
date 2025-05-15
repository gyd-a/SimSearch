#pragma once

#include <string>

#include "common/common.h"
#include "idl/gen_idl/rpc_service_idl/ps_rpc.pb.h"

class PsLocalNodeMata {
 public:
  PsLocalNodeMata();
  ~PsLocalNodeMata();

  bool Init(const std::string& dir_path);

  bool IsLoadFromFile();

  bool SetPsId(int64_t ps_node_id);
  bool SetPsIP(const std::string& ps_node_IP);
  bool SetPsPort(int32_t rpc_port);

  bool SetCreateSapceReq(const ps_rpc::CreateSpaceRequest& space_req) {
    _space_req = space_req;
    if (_space_req.space().db_name().size() > 0) {
      _has_space = true;
    }
    return true;
  }

  int64_t PsId();
  const std::string& PsIP();
  int32_t PsPort();

  std::string SpaceKey() {
    return GenSpaceKey(_space_req.space().db_name(), _space_req.space().name());
  }

  const ps_rpc::CreateSpaceRequest& SpaceReq() { return _space_req; }

  bool HasSpace() { return _has_space.load() == true ? true : false; }

  std::string DbName() {
    return _space_req.space().db_name();
  };

  std::string SpaceName() {
    return _space_req.space().name();
  };

  std::string DumpNodeInfo();
  std::string DumpCreateSpaceReq();

  std::string DeleteSpace();

 private:
  std::string _dir_path;
  std::string _node_info_file;
  std::string _space_schema_file;

  ps_rpc::PsNodeInfo _ps_node_info;
  ps_rpc::CreateSpaceRequest _space_req;

  std::atomic<bool> _has_space{false};
};
