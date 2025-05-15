
#include <gflags/gflags.h>
#include <bthread/bthread.h>
#include <brpc/channel.h>
#include <brpc/controller.h>
#include <braft/raft.h>
#include <braft/util.h>
#include <braft/route_table.h>

// DEFINE_int32(thread_num, 1, "Number of threads sending requests");
// DEFINE_bool(log_each_request, false, "Print log for each request");
// DEFINE_bool(use_bthread, false, "Use bthread to send requests");
// DEFINE_int32(write_percentage, 100, "Percentage of fetch_add");
// DEFINE_string(conf, "", "Configuration of the raft group");
// DEFINE_string(group, "Block", "Id of the replication group");


DEFINE_bool(log_each_request, false, "Print log for each request");
DEFINE_bool(use_bthread, false, "Use bthread to send requests");
DEFINE_int32(block_size, 64 * 1024 * 1024u, "Size of block");
DEFINE_int32(request_size, 1024, "Size of each requst");
DEFINE_int32(thread_num, 1, "Number of threads sending requests");
DEFINE_int32(timeout_ms, 500, "Timeout for each request");
DEFINE_int32(write_percentage, 100, "Percentage of fetch_add");
DEFINE_string(conf, "", "Configuration of the raft group");
DEFINE_string(group, "Block", "Id of the replication group");

bvar::LatencyRecorder g_latency_recorder("block_client");


int test_raft() {
  while (!brpc::IsAskedToQuit()) {
    braft::PeerId leader;
    // Select leader of the target group from RouteTable
    if (braft::rtb::select_leader(FLAGS_group, &leader) != 0) {
      std::cout << "=============select_leader failed============\n";
      // Leader is unknown in RouteTable. Ask RouteTable to refresh leader by
      // sending RPCs.
      butil::Status st = braft::rtb::refresh_leader(FLAGS_group, 500);
      if (!st.ok()) {
        // Not sure about the leader, sleep for a while and the ask again.
        LOG(WARNING) << "Fail to refresh_leader : " << st;
        bthread_usleep(100 * 1000L);
      }
      continue;
    } else {
      std::cout << "============= select_leader successful ==========, leader:"
                << leader.to_string() << "\n";
    }
    return 0;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_conf = "172.21.32.62:8200:0,";
  FLAGS_group = "Block";
  butil::AtExitManager exit_manager;
  // Register configuration of target group to RouteTable
  if (braft::rtb::update_configuration(FLAGS_group, FLAGS_conf) != 0) {
    LOG(ERROR) << "Fail to register configuration " << FLAGS_conf
                << " of group " << FLAGS_group;
    return -1;
  }





  // FLAGS_log_dir = "./logs";
  // FLAGS_logtostderr = false;
  // FLAGS_alsologtostderr = true;
  // butil::CreateDirectory(butil::FilePath(FLAGS_log_dir));
  // google::InitGoogleLogging(argv[0]);
  
  test_raft();

  /*
  LOG(INFO) << "********************* start Router **********************";
  std::string cur_IP;
  if (GetLocalIPAddress(cur_IP) == false) {
    LOG(ERROR) << "Failed to get local IP address, process exit()";
    return -1;
  }
  LOG(INFO) << "Router Current local IP address: " << cur_IP;

  LocalSpaces::GetInstance().Init(_router_store_path, "local_spaces.json");

  std::string router_node_key = GenRouterNodeKey(cur_IP, _router_rpc_port);
  std::shared_ptr<brpc::EtcdClient> etcd_cli(new brpc::EtcdClient());
  LOG(INFO) << "etcd_url: " << _etcd_url
            << ", router_node_key:" << router_node_key;
  etcd_cli->Init(_etcd_url.c_str());
  etcd_cli->RegisterNodeWithKeepAlive(router_node_key, "test_value",
                                      _router_heartbeat_interval);

  if (SpaceManager::GetInstance().Init(1000, etcd_cli).size() > 0) {
    LOG(ERROR) << "Failed to init SpaceManager";
    return -1;
  }

  std::shared_ptr<RouterServer> router_sv(new RouterServer());

  router_sv->Start();
  router_sv->WaitBrpcStop();
  */
  return 0;
}
