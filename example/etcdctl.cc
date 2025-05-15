#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/channel.h>
#include <brpc/server.h>
#include <thread>
#include <memory>
#include "butil/base64.h"
#include "google/protobuf/util/json_util.h"
#include "idl/gen_idl/etcd_idl/rpc.pb.h"
#include "clients/etcd_client.h"

DEFINE_string(url, "172.24.131.15:2370", "connect to etcd server");
DEFINE_string(api, "", "etcd api");
DEFINE_string(key, "", "key");
DEFINE_string(value, "", "value");
DEFINE_string(range_end, "", "range end");


class MyWatcher : public brpc::Watcher {
 public:
  void OnEventResponse(const ::etcdserverpb::WatchResponse& response) override {
    LOG(INFO) << response.DebugString();
  }
  void OnParseError() {
    LOG(INFO) << "parser error";
  }
};


int64_t RegisterNode(brpc::EtcdClient &client, const std::string &node_key) {
  if (node_key.empty()) {
    LOG(ERROR) << "node_key is empty";
    return -1;
  }

  ::etcdserverpb::LeaseGrantRequest grant_req;
  ::etcdserverpb::LeaseGrantResponse grant_resp;
  grant_req.set_ttl(10);
  LOG(INFO) << "LeaseGrant Request: " << grant_req.DebugString();
  client.LeaseGrant(grant_req, &grant_resp);
  LOG(INFO) << "LeaseGrant response: " << grant_resp.DebugString();
  // grant_resp.id();

  ::etcdserverpb::PutRequest request;
  ::etcdserverpb::PutResponse response;
  request.set_key(FLAGS_key);
  request.set_lease(grant_resp.id());
  request.set_value(node_key);
  LOG(INFO) << "Put Request: " << request.DebugString();
  client.Put(request, &response);
  LOG(INFO) << "Put response: " << response.DebugString();
  return grant_resp.id();
}

int ImplKeepAlive(int64_t grant_id, brpc::EtcdClient &client, int interval_time) {
  while (1) {
    ::etcdserverpb::LeaseKeepAliveRequest alive_req;
    ::etcdserverpb::LeaseKeepAliveResponse alive_resp;
    alive_req.set_id(grant_id);
    LOG(INFO) << "KeepAlive Request: " << alive_req.DebugString();
    client.LeaseKeepAlive(alive_req, &alive_resp);
    LOG(INFO) << "KeepAlive response: " << alive_resp.DebugString();
    sleep(interval_time);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  // Parse gflags. We recommend you to use gflags as well.
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  brpc::EtcdClient client;
  LOG(INFO) << "FLAGS_url " << FLAGS_url;
  const char* url = FLAGS_url.c_str();
  client.Init(url);


  int64_t grant_id = RegisterNode(client, FLAGS_key);
  ImplKeepAlive(grant_id, client, 5);


  
  if (FLAGS_api == "watch") {
    std::shared_ptr<MyWatcher> watcher(new MyWatcher);
    ::etcdserverpb::WatchRequest request;
    auto* create_request = request.mutable_create_request();
    create_request->set_key(FLAGS_key);
    if (!FLAGS_range_end.empty()) {
      create_request->set_range_end(FLAGS_range_end);
    }
    client.Watch(request, watcher);
    while (1) {
      // ctrl c to exit
      sleep(10);
    }
  } else if (FLAGS_api == "put") {
    ::etcdserverpb::PutRequest request;
    ::etcdserverpb::PutResponse response;
    request.set_key(FLAGS_key);
    if (!FLAGS_value.empty()) {
      request.set_value(FLAGS_value);
    }
    LOG(INFO) << "Put Request: " << request.DebugString();
    client.Put(request, &response);
    LOG(INFO) << "Put response: " << response.DebugString();
  } else if (FLAGS_api == "range") {
    ::etcdserverpb::RangeRequest request;
    ::etcdserverpb::RangeResponse response;
    request.set_key(FLAGS_key);
    if (!FLAGS_range_end.empty()) {
      request.set_range_end(FLAGS_range_end);
    }
    LOG(INFO) << "Range Request: " << request.DebugString();
    client.Range(request, &response);
    LOG(INFO) << "Range response: " << response.DebugString();
  } else {
    LOG(ERROR) << "unknown api " << FLAGS_api;
  }
  
  return 0;
}

