#include "service/router/space_connection.h"

#include <idl/gen_idl/rpc_service_idl/common_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/master_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/raft_rpc.pb.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common/common.h"
#include "utils/hash_algo.h"
#include "utils/one_writer_multi_reader_map.h"
#include "utils/random.h"

std::string SpaceConnection::Init(const common_rpc::Space& space) {
  _pb_space = space;
  _db_name = space.db_name();
  _space_name = space.space_name();
  _space_key = GenSpaceKey(DbName(), SpaceName());
  _partition_list.resize(_pb_space.partitions().size());
  for (int i = 0; i < _pb_space.partitions().size(); ++i) {
    auto& partition = _pb_space.partitions()[i];
    _partition_list[i] =
        std::make_unique<PartitionConnetion>(_db_name, _space_name);
    _partition_list[i]->Init(partition);
  }
  return "";
}

std::string SpaceConnection::GetDocs(
    const std::vector<std::string>& docs_key,
    std::map<std::string, common_rpc::MockDoc>& docs_res) {
  std::map<int, std::vector<std::string>> key_groups;
  for (const auto& key : docs_key) {
    int partition_idx = GetPartitionId(key, PartitionNum());
    if (key_groups.count(partition_idx) == 0) {
      key_groups[partition_idx] = std::vector<std::string>();
    }
    key_groups[partition_idx].push_back(key);
  }
  std::string msg;
  for (const auto& [p_idx, key_list] : key_groups) {
    common_rpc::MockGetResponse ps_resp;
    std::string one_msg = _partition_list[p_idx]->GetDocs(key_list, ps_resp);
    auto& docs = ps_resp.docs();
    for (auto& doc : docs) {
      docs_res[doc.key()] = doc;
    }
    if (one_msg.size() > 0) {
      msg += "partition_id:" + std::to_string(p_idx) + " query failed, ";
    }
    for (int j = 0; j < key_list.size(); ++j) {
      if (docs_res.count(key_list[j]) == 0) {
        common_rpc::MockDoc mock_doc;
        mock_doc.set_key(key_list[j]);
        mock_doc.set_msg("get doc fail.");
        docs_res[key_list[j]] = std::move(mock_doc);
        LOG(WARNING) << "key:" << key_list[j] << ", get doc fail.";
      }
    }
  }
  return msg;
}

std::map<std::string, std::string> SpaceConnection::UpsertDocs(
    const std::map<int, DocsGroup> docs_grps, std::string& msg, int test_id) {
  std::map<std::string, std::string> res_mp;
  std::map<int, std::vector<std::pair<std::string, int16_t>>> key_group;
  int64_t off = 0;
  for (const auto& [idx, docs_grp] : docs_grps) {
    auto one_grp_res = _partition_list[idx]->UpsertDocs(docs_grp, msg, test_id);
    for (int i = 0; i < docs_grp._key_vlen.size(); ++i) {
      res_mp[docs_grp._key_vlen[i].first] = one_grp_res[i];
    }
  }
  return res_mp;
}
