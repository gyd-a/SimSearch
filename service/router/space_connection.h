#pragma once

#include <idl/gen_idl/rpc_service_idl/common_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/master_rpc.pb.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common/common_type.h"
#include "service/router/partition_connection.h"
#include "utils/numeric_util.h"
#include "utils/one_writer_multi_reader_map.h"

inline uint32_t GetPartitionId(const std::string& key, int partition_num) {
  return StrHashUint64(key) % partition_num;
}

class SpaceConnection {
 public:
  inline const std::string& SpaceName() { return _space_name; };
  inline const std::string& DbName() { return _db_name; };
  inline int32_t PartitionNum() { return _pb_space.partition_num(); };
  inline int32_t ReplicaNum() { return _pb_space.replica_num(); };
  inline void Partitions() { _pb_space.partitions(); };
  inline const common_rpc::Space& PbSpace() { return _pb_space; };
  inline const std::string& SpaceKey() { return _space_key; };

  std::string Init(const common_rpc::Space& space);

  std::map<std::string, std::string> UpsertDocs(const std::map<int, DocsGroup> docs_grps,
                                                std::string& msg, int test_id);

  std::map<std::string, std::string> DeleteDocs(
      const std::map<int, std::vector<std::string>>& idx_keys_mp, std::string& msg);

  std::map<std::string, std::string> GetDocs(
      const std::map<int, std::vector<std::string>>& idx_keys_mp,
      std::map<std::string, common_rpc::Document>& docs_res);

  DType id_type() { return _id_type; }

  const common_rpc::Space& pb_space() { return _pb_space; };

 private:
  common_rpc::Space _pb_space;
  std::string _space_key;
  std::vector<std::unique_ptr<PartitionConnetion>> _partition_list;
  std::string _db_name;
  std::string _space_name;
  DType _id_type = DType::UNDEFINE;
};
