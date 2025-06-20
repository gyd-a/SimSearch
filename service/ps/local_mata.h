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

  bool SetCreateSapceReq(const ps_rpc::CreateSpaceRequest& space_req);

  std::string SpaceKey();

  bool SetPsId(int64_t ps_node_id);
  bool SetPsIP(const std::string& ps_node_IP);
  bool SetPsPort(int32_t rpc_port);
  int64_t Id();
  const std::string& IP();
  int32_t Port();
  const ps_rpc::CreateSpaceRequest& SpaceReq();
  bool HasSpace();
  std::string DbName();
  std::string SpaceName();
  std::string DumpNodeInfo();
  std::string DumpCreateSpaceReq();
  std::string DeleteSpace();
  std::string DeleteNodeInfo();

 private:
  std::string _dir_path;
  std::string _node_info_file;
  std::string _space_schema_file;

  ps_rpc::PsNodeInfo _ps_node_info;
  ps_rpc::CreateSpaceRequest _space_req;

  std::atomic<bool> _has_space{false};
};
