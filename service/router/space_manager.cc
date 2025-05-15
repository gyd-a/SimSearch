#include "service/router/space_manager.h"

#include <google/protobuf/util/json_util.h>
#include <idl/gen_idl/rpc_service_idl/common_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/master_rpc.pb.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "clients/ps_rpc_client.h"
#include "config/conf.h"
#include "raft_store/raft_client.h"
#include "utils/one_writer_multi_reader_map.h"
#include "utils/random.h"

std::string SpaceManager::Init(int16_t max_space_num,
                               std::shared_ptr<brpc::EtcdClient>& etcd_client) {
  std::string msg;
  _spaces_conn.reserve(max_space_num);
  this->_etcd_client = etcd_client;
  ::etcdserverpb::RangeRequest req;
  ::etcdserverpb::RangeResponse resp;
  req.set_key(_etcd_space_prefix);
  req.set_range_end(brpc::GetRangeEnd(_etcd_space_prefix));
  // req.set_limit(0);
  req.set_sort_order(etcdserverpb::RangeRequest::ASCEND);
  req.set_sort_target(etcdserverpb::RangeRequest::KEY);
  if (etcd_client->Range(req, &resp) == false) {
    msg = "Init SpaceManager fail, get space info from etcd failed";
    LOG(ERROR) << msg;
    return msg;
  }
  // LOG(INFO) << "Range req: " << req.DebugString() << " resp:" <<
  // resp.DebugString();

  std::vector<common_rpc::Space> spaces_info;
  if (resp.kvs_size() == 0) {
    LocalSpaces::GetInstance().Load();
    spaces_info = LocalSpaces::GetInstance().GetSpaces();
    LOG(INFO) << "No space info in etcd, Use local spaces info, size:"
              << spaces_info.size();
  } else {
    const auto& info_kvs = resp.kvs();
    for (int i = 0; i < info_kvs.size(); ++i) {
      common_rpc::Space pb_space;
      msg = SpaceJsonToPb(info_kvs[i].value(), pb_space);
      if (msg.size() > 0) {
        LOG(ERROR) << "Init SpaceManager fail, space_key:" << info_kvs[i].key()
                   << " " << msg;
        return msg;
      }
      LOG(INFO) << "Get space info from etcd, i:" << i
                << ", key:" << info_kvs[i].key()
                << " value:" << info_kvs[i].value();
      spaces_info.push_back(pb_space);
    }
    LocalSpaces::GetInstance().SetSpaces(spaces_info);
    LocalSpaces::GetInstance().Save();
  }
  std::string spaces_key_log;
  std::lock_guard<std::mutex> lock(_mtx);  // 自动加锁和解锁
  for (int i = 0; i < spaces_info.size(); ++i) {
    auto conn = std::make_shared<SpaceConnection>();
    conn->Init(spaces_info[i]);
    _spaces_conn.push_back(conn);
    _space_idx_mp.Put(conn->SpaceKey(), i);
    spaces_key_log += conn->SpaceKey() + ",";
  }
  LOG(INFO) << "Init SpaceManager success, spaces num:" << _spaces_conn.size()
            << ", spaces_key:" << spaces_key_log;
  return msg;
}

std::string SpaceManager::AddSpace(const common_rpc::Space& pb_space) {
  std::string msg;
  std::lock_guard<std::mutex> lock(_mtx);  // 自动加锁和解锁
  auto conn = std::make_shared<SpaceConnection>();
  conn->Init(pb_space);
  if (_space_idx_mp.Get(conn->SpaceKey()) >= 0) {
    msg = "space_key:" + conn->SpaceKey() + " is already exist, not add.";
    LOG(ERROR) << msg;
    return msg;
  }
  for (int i = 0; i < _spaces_conn.size(); ++i) {
    if (_spaces_conn[i] == nullptr) {
      _spaces_conn[i] = conn;
      _space_idx_mp.Put(conn->SpaceKey(), i);
      return msg;
    }
  }
  if (_spaces_conn.size() >= _spaces_conn.capacity()) {
    msg = "spaces num > max_space_num[" +
          std::to_string(_spaces_conn.capacity()) + "] create space failed";
    return msg;
  }
  _spaces_conn.push_back(conn);
  _space_idx_mp.Put(conn->SpaceKey(), _spaces_conn.size() - 1);
  LocalSpaces::GetInstance().AddSpace(pb_space);
  LocalSpaces::GetInstance().Save();
  return msg;
}

std::string SpaceManager::DeleteSpace(const std::string& space_key) {
  std::string msg;
  std::lock_guard<std::mutex> lock(_mtx);
  int16_t idx = _space_idx_mp.Get(space_key);
  if (idx >= 0) {
    _space_idx_mp.Delete(space_key);
    _spaces_conn[idx] = nullptr;
    LOG(INFO) << "Delete space:[" << space_key << "] is successful.";
  } else {
    msg = "space_key:" + space_key + " is not exit, not delete.";
    LOG(ERROR) << msg;
  }
  return msg;
}

std::shared_ptr<SpaceConnection> SpaceManager::GetSpaceConn(
    const std::string& space_key, std::string& msg) {
  if (space_key.size() <= 1) {
    msg = "db_name and space_name is null";
    return nullptr;
  }
  std::shared_ptr<SpaceConnection> space_conn;
  int space_idx = _space_idx_mp.Get(space_key);
  if (space_idx < 0) {
    msg = "db_name and space_name [" + space_key + "] is not exist";
    LOG(ERROR) << msg;
    return space_conn;
  }
  space_conn = _spaces_conn[space_idx];
  if (space_conn == nullptr) {
    msg = "db_name and space_name  [" + space_key + "] is deleted.";
    LOG(WARNING) << msg;
    return space_conn;
  }
  return space_conn;
}

std::map<std::string, common_rpc::MockDoc> SpaceManager::GetDocsFromPs(
    const std::string& space_key, const std::vector<std::string>& keys,
    std::string& msg) {
  std::map<std::string, common_rpc::MockDoc> docs_res;
  auto space_conn = GetSpaceConn(space_key, msg);
  if (msg.size() > 0 || space_conn == nullptr) {
    LOG(ERROR) << "space_key:" << space_key << " is not exist.";
    return docs_res;
  };
  msg = space_conn->GetDocs(keys, docs_res);
  return docs_res;
}

std::map<std::string, std::string> SpaceManager::UpsertDocsToPs(
    const std::string& space_key, const std::map<int, DocsGroup>& docs_groups,
    std::string& msg, int test_id) {
  auto space_conn = GetSpaceConn(space_key, msg);
  if (msg.size() > 0 || space_conn == nullptr) {
    return {};
  };
  return space_conn->UpsertDocs(docs_groups, msg, test_id);
}

std::map<int, DocsGroup> SpaceManager::SplitDocs(
    const google::protobuf::RepeatedPtrField<common_rpc::MockDoc>& docs,
    int partition_num) {
  std::map<int, DocsGroup> idx_to_docs_groups;
  for (const auto& doc : docs) {
    int partition_id = GetPartitionId(doc.key(), partition_num);
    if (idx_to_docs_groups.count(partition_id) == 0) {
      idx_to_docs_groups[partition_id] = DocsGroup();
      idx_to_docs_groups[partition_id]._partition_id = partition_id;
    }
    auto& docs_grp = idx_to_docs_groups[partition_id];
    docs_grp._data_len += doc.val().size();
    ++docs_grp._doc_num;
  }
  for (auto& [idx, docs_group] : idx_to_docs_groups) {
    docs_group._data.reserve(docs_group._data_len);
    docs_group._key_vlen.reserve(docs_group._doc_num);
  }
  for (const auto& doc : docs) {
    int partition_id = GetPartitionId(doc.key(), partition_num);
    auto& docs_grp = idx_to_docs_groups[partition_id];
    docs_grp._data += doc.val();
    docs_grp._key_vlen.emplace_back(doc.key(), doc.val().size());
  }
  return idx_to_docs_groups;
}

std::string SpaceManager::SpaceJsonToPb(const std::string& space_json,
                                        common_rpc::Space& pb_space) {
  std::string msg;
  google::protobuf::util::JsonParseOptions opts;
  opts.ignore_unknown_fields = true;  // 如果json有多余字段则忽略
  google::protobuf::util::Status status =
      google::protobuf::util::JsonStringToMessage(space_json, &pb_space, opts);
  if (!status.ok()) {
    msg += "Failed to parse space JSON: " + status.ToString() +
           " space_json:" + space_json;
    return msg;
  }
  return msg;
}

SpaceManager& SpaceManager::GetInstance() {
  static SpaceManager space_mgr;
  return space_mgr;
}
