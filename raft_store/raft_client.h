#pragma once

#include <braft/raft.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

// #include "idl/gen_idl/raft_store_idl/raft_store_idl.pb.h"  // BlockService
#include "common/brpc_options.h"
#include "idl/gen_idl/rpc_service_idl/raft_rpc.pb.h"

class RaftClient {
 public:
  std::string Write(const raft_rpc::BlockRequest& req,
                    raft_rpc::BlockResponse& resp, const std::string& data);

  std::string GetLeader(braft::PeerId& leader, const std::string& group,
                        const std::string& conf, int retries);

  std::string CreateRaftConnection(const std::string& group,
                                   const std::string& conf, int new_idx);

  std::string Init(const std::string& group, const std::string& conf);

 private:
  int _timeout_ms = 500;  // ms
  std::vector<RaftOptions> _raft_options;
  std::mutex _mtx;
  std::atomic<int> _idx{0};
};

/*
class RaftClientManager {
 public:
  ~RaftClientManager() {
    for (auto& [name, raft_cli] : group_to_conf_) {
      if (raft_cli) {
        delete raft_cli;
        group_to_conf_[name] = nullptr;
      }
    }
  }

  std::string AddOrUpdateRaftGroup(const std::string& group,
                                   const std::string& conf) {
    if (group_to_conf_.count(group) == true) {
      LOG(ERROR) << "group: " << group << " is exist !!!";
      return "";
    }
    auto raft_cli = new RaftClient();
    group_to_conf_[group] = raft_cli;
    std::string err_str = raft_cli->Init(group, conf);
    if (err_str.size() > 0) {
      return err_str;
    }
    return "";
  };

 private:
  std::map<std::string, RaftClient*> group_to_conf_;
};
*/
