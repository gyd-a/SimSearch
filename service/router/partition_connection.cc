#include "service/router/partition_connection.h"

#include <idl/gen_idl/rpc_service_idl/common_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/master_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/raft_rpc.pb.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "clients/ps_rpc_client.h"
#include "common/common.h"
#include "raft_store/raft_client.h"
#include "utils/hash_algo.h"
#include "utils/one_writer_multi_reader_map.h"
#include "utils/random.h"


PartitionOpt::PartitionOpt(const common_rpc::Partition& pb_partition) {
  _pb_partition = pb_partition;
  _replicas_num = pb_partition.replicas().size();
  _ps_list.resize(_replicas_num);
}

std::string PartitionOpt::Init() {
  const auto& replicas = _pb_partition.replicas();
  std::string msg;
  for (int i = 0; i < replicas.size(); ++i) {
    const auto& rep = replicas[i];
    auto addr_port = rep.ps_ip() + ":" + std::to_string(rep.ps_port());
    auto one_msg = _ps_list[i].Init(addr_port, _pb_partition.group_name(), rep.ps_id());
    if (one_msg.size() > 0) {
      msg += "replica_id:" + std::to_string(i) + ", " + one_msg + "; ";
      LOG(ERROR) << msg;
    }
  }
  return msg;
}

// **********************************************************************

uint32_t PartitionConnetion::GetReplicaIdx(int replicas_num) {
  int max_v = replicas_num < 1 ? 1 : replicas_num;
  return (uint32_t)GenRandomInt(1, max_v) - 1;
}

std::string PartitionConnetion::Init(
    const common_rpc::Partition& pb_partition) {
  std::string msg;
  const auto& replicas = pb_partition.replicas();
  _partition_opt.resize(2);
  if (replicas.size() == 0) {
    msg = "partition group_name:" + pb_partition.group_name() +
          "replicas num is 0";
    LOG(ERROR) << msg;
    return msg;
  }
  for (int j = 0; j < 2; ++j) {
    _partition_opt[j] = std::make_shared<PartitionOpt>(pb_partition);
    _partition_opt[j]->Init();
  }
  _idx = 0;
  msg = _raft_client.Init(pb_partition.group_name(), GetRaftConf(pb_partition));
  return msg;
}

std::string PartitionConnetion::GetDocs(
    const std::vector<std::string>& doc_keys,
    common_rpc::MockGetResponse& ps_resp) {
  std::string one_msg;
  for (int i = 0; i < _retries; ++i) {
    auto partition_opt = _partition_opt[_idx];
    one_msg.clear();
    ps_resp.Clear();
    if (partition_opt == nullptr) {
      one_msg = "***** two buffer partion_opt is null *****";
      return one_msg;
    }
    uint32_t idx = GetReplicaIdx(partition_opt->_replicas_num);
    auto& ps_cli = partition_opt->_ps_list[idx];
    one_msg = ps_cli.MockGet(doc_keys, ps_resp);
    if (one_msg.size() == 0) {
      LOG(INFO) << "****** Router GetDocs from Ps success ******";
      return one_msg;
    }
  }
  LOG(ERROR) << "Router GetDocs from Ps error, msg:" << one_msg;
  return one_msg;
}

std::vector<std::string> PartitionConnetion::UpsertDocs(
    const DocsGroup& docs_grp, std::string& msg, int test_id) {
  raft_rpc::BlockRequest req;
  raft_rpc::BlockResponse resp;
  req.set_test_id(test_id);
  req.set_db_name(_db_name);
  req.set_space_name(_space_name);
  req.set_write_type(raft_rpc::WriteType::UPSERT);
  req.set_key_type(raft_rpc::KeyType::STRING);
  auto* doc_infos = req.mutable_docs_info();
  for (const auto& [key, doc_len] : docs_grp._key_vlen) {
    auto* doc_info = doc_infos->Add();
    doc_info->set_key(key);
    doc_info->set_doc_len(doc_len);
  }
  std::vector<std::string> msgs(docs_grp._key_vlen.size());
  msg = _raft_client.Write(req, resp, docs_grp._data);
  const auto& ret_docs_info = resp.docs_info();
  for (int i = 0; i < ret_docs_info.size(); ++i) {
    if (ret_docs_info[i].msg().size() > 0) {
      LOG(ERROR) << "db_name:" << _db_name << ", space_name:" << _space_name
                 << ", upsert doc key:" << ret_docs_info[i].key()
                 << " failed, msg:" << ret_docs_info[i].msg();
    }
    msgs[i] = ret_docs_info[i].msg();
  }
  return msgs;
}

std::string PartitionConnetion::UpdatePsConn(
    const common_rpc::Partition& pb_partition) {
  std::string msg;
  const auto& replicas = pb_partition.replicas();
  if (replicas.size() == 0) {
    msg = "partition group_name:" + pb_partition.group_name() +
          "replicas num is 0";
    LOG(ERROR) << msg;
    return msg;
  }
  int new_ps_idx = (_idx + 1) % 2;
  auto new_part_opt = std::make_shared<PartitionOpt>(pb_partition);
  msg = new_part_opt->Init();
  if (msg.size() == 0) {
    _partition_opt[new_ps_idx] = new_part_opt;
    _idx = new_ps_idx;
  }
  return msg;
}
