

#include <gflags/gflags.h>
#include <bthread/bthread.h>
#include <brpc/channel.h>
#include <brpc/controller.h>
#include <braft/raft.h>
#include <braft/util.h>
#include <braft/route_table.h>

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

static void* sender(void* arg) {
    while (!brpc::IsAskedToQuit()) {
        braft::PeerId leader;
        // Select leader of the target group from RouteTable
        if (braft::rtb::select_leader(FLAGS_group, &leader) != 0) {
            std::cout << "=============select_leader failed============\n";
            // Leader is unknown in RouteTable. Ask RouteTable to refresh leader by sending RPCs.
            butil::Status st = braft::rtb::refresh_leader(FLAGS_group, FLAGS_timeout_ms);
            if (!st.ok()) {
                // Not sure about the leader, sleep for a while and the ask again.
                LOG(WARNING) << "Fail to refresh_leader : " << st;
                bthread_usleep(FLAGS_timeout_ms * 1000L);
            }
            continue;
        }
        std::cout << "FLAGS_group:[" << FLAGS_group << "]==  leader.addr: " << leader.addr << "  ===\n";
        return nullptr;
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    butil::AtExitManager exit_manager;
    FLAGS_group = "Block";
    FLAGS_conf = "172.21.32.62:8200:0,";

    // Register configuration of target group to RouteTable
    if (braft::rtb::update_configuration(FLAGS_group, FLAGS_conf) != 0) {
        LOG(ERROR) << "Fail to register configuration " << FLAGS_conf
                   << " of group " << FLAGS_group;
        return -1;
    }
    sender(&argc);  
    return 0;
}

