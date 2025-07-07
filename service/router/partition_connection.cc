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
#include "utils/numeric_util.h"
#include "utils/one_writer_multi_reader_map.h"
#include "utils/numeric_util.h"
#include "utils/log.h"


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

/*
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
*/

std::map<std::string, std::string> PartitionConnetion::UpsertDocs(
    const DocsGroup& docs_grp, std::string& msg, int test_id) {
  std::map<std::string, std::string> err_docs;
  raft_rpc::BlockRequest req;
  req.set_db_name(_db_name);
  req.set_space_name(_space_name);
  req.set_impl_type(raft_rpc::ImplType::UPSERT);
  req.set_key_type(raft_rpc::KeyType::STRING);
  for (const auto& one : docs_grp._pb_doc_infos) {
    std::string binary, one_msg;
    if (!one._pb_doc->SerializeToString(&binary)) {
      one_msg = "SerializeToString Document failed";
      LOG(ERROR) << one_msg;
      err_docs[one._key] = one_msg;
      continue;
    }
    req.set_key(one._key);
    raft_rpc::BlockResponse resp;
    one_msg = _raft_client.Write(req, resp, binary);
    if (one_msg.size() > 0 || resp.success() == false) {
      LOG(ERROR) << "raft Write error, imp_msg:" << one_msg << ", ret_msg:" 
                 << resp.msg();
      err_docs[one._key] = one_msg + ", ret_msg:" + resp.msg();
      continue;
    }
    LOG(INFO) << "***** raft Write success, key:" << one._key << " *******";
  }
  return err_docs;
}

std::map<std::string, std::string> PartitionConnetion::DeleteDocs(
    const std::vector<std::string>& doc_keys, std::string& msg) {
  std::map<std::string, std::string> err_ids;
  for (const auto& one_key : doc_keys) {
    
  }

  return err_ids;
}


std::map<std::string, std::string> PartitionConnetion::GetDocs(
    const std::vector<std::string>& doc_keys,
    std::map<std::string, common_rpc::Document>& docs_mp) {
  std::map<std::string, std::string> err_docs;
  auto add_err_msg = [&err_docs, doc_keys](const std::string& msg) {
    for (const auto& key : doc_keys) {
      err_docs[key] = msg;
    }
  };
  auto partition_opt = _partition_opt[_idx];
  std::string one_msg;
  if (partition_opt == nullptr) {
    one_msg = "***** two buffer partion_opt is null *****";
    add_err_msg(one_msg);
    return err_docs;
  }
  int replica_idx = -1;
  for (int i = 0; i < _retries; ++i) {
    if (replica_idx == -1) {
      replica_idx = GetReplicaIdx(partition_opt->_replicas_num);
    } else {
      replica_idx = (++replica_idx) % partition_opt->_replicas_num;
    }
    auto& ps_cli = partition_opt->_ps_list[replica_idx];
    common_rpc::GetResponse ps_resp;
    one_msg = ps_cli.Get(doc_keys, ps_resp);
    if (one_msg.size() == 0) {
      std::map<std::string, int> succ_key_to_idx, failed_key_to_idx;
      for (int i = 0; i < ps_resp.documents_size(); ++i) {
        const auto& doc = ps_resp.documents(i);
        succ_key_to_idx[doc._id()] = i;
      }
      for (int i = 0; i < ps_resp.failed_docs_size(); ++i) {
        const auto& doc = ps_resp.failed_docs(i);
        failed_key_to_idx[doc._id()] = i;
      }
      for (int i = 0; i < doc_keys.size(); ++i) {
        const auto& key = doc_keys[i];
        if (succ_key_to_idx.find(key) != succ_key_to_idx.end()) {
          docs_mp[key] = ps_resp.documents(succ_key_to_idx[key]);
        } else if (failed_key_to_idx.find(key) != failed_key_to_idx.end()) {
          err_docs[key] = ps_resp.failed_docs(failed_key_to_idx[key]).msg();
        } else {
          err_docs[key] = "Key not found in response";
        }
      }
      LOG(INFO) << "****** GetDocs from Ps success ******";
      return err_docs;
    } else {
      LOG(ERROR) << "GetDocs from Ps error, msg:" << one_msg
                 << ", replica_idx:" << replica_idx;
    }
  }
  add_err_msg("GetDocs from Ps error, msg:" + one_msg);
  LOG(ERROR) << "Router GetDocs from Ps error, msg:" << one_msg;
  return err_docs;
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
