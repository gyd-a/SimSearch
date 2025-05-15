#include "raft_store/raft_client.h"

#include <braft/raft.h>
#include <braft/route_table.h>
#include <braft/util.h>
#include <brpc/channel.h>
#include <brpc/controller.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>


// #include "idl/gen_idl/raft_store_idl/raft_store_idl.pb.h"  // BlockService
#include "common/brpc_options.h"
#include "idl/gen_idl/rpc_service_idl/raft_rpc.pb.h"

std::string RaftClient::Write(const raft_rpc::BlockRequest& req,
                              raft_rpc::BlockResponse& resp,
                              const std::string& data) {
  std::string msg;
  std::lock_guard<std::mutex> lock(_mtx);
  for (int i = 0; i < 3; ++i) {
    resp.Clear();
    auto& raft_opt = _raft_options[_idx];
    if (raft_opt.ok_ == false) {
      CreateRaftConnection(raft_opt.group_, raft_opt.conf_, (_idx + 1) % 2);
      continue;
    }
    brpc::Controller cntl;
    cntl.set_timeout_ms(_timeout_ms * 2);
    // Randomly select which request we want send;
    cntl.request_attachment().append(data);
    raft_opt.stub_->write(&cntl, &req, &resp, NULL);
    if (cntl.Failed()) {
      msg = ("Fail to send request to " + raft_opt.leader_.to_string() +
             " : " + cntl.ErrorText());
      LOG(ERROR) << msg;
      // Clear leadership since this RPC failed.
      braft::rtb::update_leader(raft_opt.group_, braft::PeerId());
      CreateRaftConnection(raft_opt.group_, raft_opt.conf_, (_idx + 1) % 2);
      continue;
    }
    if (!resp.success()) {
      msg = ("Fail to send request to " + raft_opt.leader_.to_string() +
             ", redirecting to " +
             (!resp.redirect().empty() ? resp.redirect() : "nowhere"));
      LOG(ERROR) << msg << " response:" << resp.DebugString();
      // Update route table since we have redirect information
      braft::rtb::update_leader(raft_opt.group_, resp.redirect());
      CreateRaftConnection(raft_opt.group_, raft_opt.conf_, (_idx + 1) % 2);
      continue;
    }
    LOG(INFO) << "Success write To Raft, leader:" << raft_opt.leader_
              << " latency=" << cntl.latency_us();
    return "";
  }
  return msg;
}

std::string RaftClient::GetLeader(braft::PeerId& leader,
                                  const std::string& group,
                                  const std::string& conf, int retries) {
  bool is_succ = false;
  std::string msg;
  for (int i = 0; i < retries; ++i) {
    // 通过第一次会失败，第二次会成功
    if (braft::rtb::select_leader(group, &leader) != 0) {
      butil::Status st = braft::rtb::refresh_leader(group, _timeout_ms);
      if (!st.ok()) {
        // Not sure about the leader, sleep for a while and the ask again.
        LOG(WARNING) << "Fail to refresh_leader:[" << st << "] group:[" << group
                     << "] conf:[" << conf << "]";
      }
      bthread_usleep((_timeout_ms / 10) * 1000L);
      continue;
    }
    is_succ = true;
    break;
  }
  if (is_succ == false) {
    msg = "Fail to select leader of group:[" + group + "] conf:[" + conf + "]";
    LOG(ERROR) << msg;
    return msg;
  }
  LOG(INFO) << "Success select leader of group:[" + group + "] conf:" + conf;
  return "";
}

std::string RaftClient::CreateRaftConnection(const std::string& group,
                                             const std::string& conf,
                                             int new_idx) {
  std::string msg;
  auto& raft_opt = _raft_options[new_idx];
  raft_opt.ok_ = false;
  raft_opt.group_ = group;
  raft_opt.conf_ = conf;
  if (!brpc::IsAskedToQuit()) {
    msg = GetLeader(raft_opt.leader_, group, conf, 3);
    if (msg.size() > 0) {
      return msg;
    }
    if (raft_opt.Build(NULL) == false) {
      msg = "Fail to connet brpc, leader:" + raft_opt.leader_.to_string();
      LOG(ERROR) << msg;
      return msg;
    }
    _idx = new_idx;
    raft_opt.ok_ = true;
    LOG(INFO) << "Success to connect braft leader, conf:[" << conf
              << "], group:[" << group << "]";
    return msg;
  }
  msg = "Brpc IsAskedToQuit, not update raft connection.";
  LOG(ERROR) << msg;
  return msg;
}

std::string RaftClient::Init(const std::string& group,
                             const std::string& conf) {
  std::string msg;
  std::lock_guard<std::mutex> lock(_mtx);
  _raft_options.resize(2);
  // Register configuration of target group to RouteTable
  if (braft::rtb::update_configuration(group, conf) != 0) {
    LOG(ERROR) << "Fail to register configuration  conf:[" << conf << "] of group:["
               << group << "]";
    return "group:" + group + "update_configuratione error";
  }
  for (int i = 0; i < 2; ++i) {
    msg = CreateRaftConnection(group, conf, i);
    if (msg.size() > 0) {
      return "Fail to get leader";
    }
  }
  LOG(INFO) << "RaftClient Success init, conf:[" << conf << "], group:["
            << group << "]";
  return "";
}
