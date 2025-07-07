#pragma once

#include <idl/gen_idl/rpc_service_idl/common_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/master_rpc.pb.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <map>

#include "clients/ps_rpc_client.h"
#include "raft_store/raft_client.h"
#include "utils/one_writer_multi_reader_map.h"

struct PbDocInfo {
  PbDocInfo(const std::string& key, const common_rpc::Document* pb_doc): _key(key), _pb_doc(pb_doc) {}
  std::string _key;
  const common_rpc::Document* _pb_doc;
  // std::string binary;
};

struct DocsGroup {
  int _partition_id;
  std::vector<PbDocInfo> _pb_doc_infos;   // _id, doc
  std::string _data;
};


struct PartitionOpt {
  common_rpc::Partition _pb_partition;
  std::vector<PsRpcClient> _ps_list;  // 用智能指针
  std::atomic<int8_t> _replicas_num;
  PartitionOpt(const common_rpc::Partition& pb_partition);
  std::string Init();
};

class PartitionConnetion {
 public:
  PartitionConnetion(const std::string& db_name, const std::string& space_name)
      : _db_name(db_name), _space_name(space_name){};

  std::string Init(const common_rpc::Partition& pb_partition);

  // std::string GetDocs(const std::vector<std::string>& doc_keys,
  //                     common_rpc::MockGetResponse& ps_resp);

  std::map<std::string, std::string> UpsertDocs(const DocsGroup& docs_grp,
                                                std::string& msg, int test_id);

  std::map<std::string, std::string> DeleteDocs(const std::vector<std::string>& doc_keys,
                                                std::string& msg);

  std::map<std::string, std::string> GetDocs(const std::vector<std::string>& doc_keys,
      std::map<std::string, common_rpc::Document>& docs_mp);

  uint32_t GetReplicaIdx(int replicas_num);

  std::string UpdatePsConn(const common_rpc::Partition& pb_partition);

 private:
  std::vector<std::shared_ptr<PartitionOpt>> _partition_opt;
  std::atomic<int8_t> _idx{0};
  RaftClient _raft_client;
  const std::string& _db_name;
  const std::string& _space_name;
  int _retries = 3;
};
