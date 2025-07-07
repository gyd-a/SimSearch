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
#include "common/common_type.h"

#include "utils/numeric_util.h"
#include "utils/one_writer_multi_reader_map.h"
#include "utils/numeric_util.h"
#include "utils/log.h"


std::string SpaceConnection::Init(const common_rpc::Space& space) {
  _pb_space = space;
  _db_name = space.db_name();
  _space_name = space.space_name();
  _space_key = GenSpaceKey(DbName(), SpaceName());
  _partition_list.resize(_pb_space.partitions().size());
  std::string msg;
  for (int i = 0; i < _pb_space.partitions().size(); ++i) {
    auto& partition = _pb_space.partitions()[i];
    _partition_list[i] =
        std::make_unique<PartitionConnetion>(_db_name, _space_name);
    _partition_list[i]->Init(partition);
  }
  for (int i = 0; i < space.fields().size(); ++i) {
    const auto& fie = space.fields(i);
    std::string fie_type = fie.type();
    if (fie.name() == _ID) { 
      _id_type = SDTypeToDType(fie_type);
      if (!IsIdDType(_id_type)) {
        _id_type = DType::UNDEFINE;
        msg = "_id type:";
        msg += fie_type + " is not allowed, it only is string or long type.";
        LOG(ERROR) << msg;
        return msg;
      }
    }
  }
  if (_id_type == DType::UNDEFINE) {
    msg = "not has _id field.";
    LOG(ERROR) << msg;
    return msg;
  }
  return "";
}

/*
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
}*/

std::map<std::string, std::string> SpaceConnection::UpsertDocs(
    const std::map<int, DocsGroup> docs_grps, std::string& msg, int test_id) {
  std::map<std::string, std::string> err_mp;
  for (const auto& [idx, docs_grp] : docs_grps) {
    auto one_grp_res = _partition_list[idx]->UpsertDocs(docs_grp, msg, test_id);
    for (const auto& [key, one_msg]: one_grp_res) {
      err_mp[key] = one_msg;
    }
  }
  return err_mp;
}

std::map<std::string, std::string> SpaceConnection::DeleteDocs(
    const std::map<int, std::vector<std::string>>& idx_keys_mp,
    std::string& msg) {
  std::map<std::string, std::string> err_mp;
  for (const auto& [idx, keys] : idx_keys_mp) {
    auto one_grp_res = _partition_list[idx]->DeleteDocs(keys, msg);
    for (const auto& [key, one_msg]: one_grp_res) {
      err_mp[key] = one_msg;
    }
  }
  return err_mp;
}

std::map<std::string, std::string> 
SpaceConnection::GetDocs(const std::map<int, std::vector<std::string>>& idx_keys_mp,
                         std::map<std::string, common_rpc::Document>& docs_res) {
  std::map<std::string, std::string> err_mp;
  for (const auto& [idx, keys] : idx_keys_mp) {
    std::map<std::string, common_rpc::Document> one_grp_res;
    auto one_grp_err = _partition_list[idx]->GetDocs(keys, one_grp_res);
    for (auto& [key, one_err]: one_grp_err) {
      err_mp[key] = std::move(one_err);
    }
    for (auto& [key, one_res]: one_grp_res) {
      docs_res[key] = std::move(one_res);
    }
  }
  return err_mp;
}