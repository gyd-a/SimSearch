#ifndef SRC_ETCD_CLIENT_ETCD_CLIENT_H_
#define SRC_ETCD_CLIENT_ETCD_CLIENT_H_
#include <string>
#include <memory>
#include "brpc/channel.h"
#include "idl/gen_idl/etcd_idl/rpc.pb.h"
#include "idl/gen_idl/etcd_idl/http_api.pb.h"

#include "butil/endpoint.h"

namespace brpc {


// The routine of Watcher will be invoked by different event read thread
// Users should ensure thread safe on the process of event
class Watcher {
 public:
  virtual void OnEventResponse(const ::etcdserverpb::WatchResponse& response) {}
  virtual void OnParseError() {}
};
typedef std::shared_ptr<Watcher> WatcherPtr;

class EtcdClient;
struct KeepAliveArgs {
    EtcdClient* etcd_cli_;
    std::string key_;
    std::string val_;
    int interval_sec_;
};

class EtcdClient {
 public:
  EtcdClient();
  ~EtcdClient();
//   bool Init(const std::string& url, const std::string& lb = "rr");
//   bool Init(butil::EndPoint server_addr_and_port);
//   bool Init(const char* server_addr_and_port);
//   bool Init(const char* server_addr, int port);


  bool Init(const std::vector<std::pair<std::string, int>>& ip_ports, 
            const std::string& lb = "rr");

  bool Put(const ::etcdserverpb::PutRequest& request,
      ::etcdserverpb::PutResponse* response);

  bool Range(const ::etcdserverpb::RangeRequest& request,
      ::etcdserverpb::RangeResponse* response);

  bool DeleteRange(const ::etcdserverpb::DeleteRangeRequest& request,
      ::etcdserverpb::DeleteRangeResponse* response);

  bool Txn(const ::etcdserverpb::TxnRequest& request,
      ::etcdserverpb::TxnResponse* response);

  bool Compact(const ::etcdserverpb::CompactionRequest& request,
      ::etcdserverpb::CompactionResponse* response);

  bool LeaseGrant(const ::etcdserverpb::LeaseGrantRequest& request,
      ::etcdserverpb::LeaseGrantResponse* response);

  bool LeaseRevoke(const ::etcdserverpb::LeaseRevokeRequest& request,
      ::etcdserverpb::LeaseRevokeResponse* response);

  bool LeaseTimeToLive(const ::etcdserverpb::LeaseTimeToLiveRequest& request,
      ::etcdserverpb::LeaseTimeToLiveResponse* response);

  bool LeaseLeases(const ::etcdserverpb::LeaseLeasesRequest& request,
      ::etcdserverpb::LeaseLeasesResponse* response);

  bool LeaseKeepAlive(const ::etcdserverpb::LeaseKeepAliveRequest& request,
      ::etcdserverpb::HttpLeaseKeepAliveResponse* response);

  bool Watch(const ::etcdserverpb::WatchRequest& request, WatcherPtr watcher);

  int RegisterNodeWithKeepAlive(const std::string& node_key, const std::string& node_val,
                                int interval_second);

  static void* KeepNodeAliveThreadFunc(void* args);
  
  bool _keep_alive_bt_running = false;
 private:
//   brpc::Channel _channel;
  bthread_t _keep_alive_bt;
  std::vector<std::unique_ptr<brpc::Channel>> _chan_list;
  int _chan_size;
};

std::string GetRangeEnd(const std::string& key);


}  // namespace brpc

#endif  // SRC_ETCD_CLIENT_ETCD_CLIENT_H_
