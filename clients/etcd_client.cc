#include "clients/etcd_client.h"

#include <unistd.h>

#include <memory>

#include "butil/string_splitter.h"
#include "json2pb/json_to_pb.h"
#include "json2pb/pb_to_json.h"
#include "utils/random.h"

namespace brpc {

EtcdClient::EtcdClient() {}

EtcdClient::~EtcdClient() {
  if (_keep_alive_bt_running) {
    _keep_alive_bt_running = false;
    bthread_join(_keep_alive_bt, nullptr);
  }
}

/*
bool EtcdClient::Init(const std::string& url, const std::string& lb) {
  brpc::ChannelOptions options;
  options.protocol = "http";
  if (_channel.Init(url.c_str(), lb.c_str(), &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel with " << url;
    return false;
  }
  return true;
}

bool EtcdClient::Init(butil::EndPoint server_addr_and_port) {
  brpc::ChannelOptions options;
  options.protocol = "http";
  if (_channel.Init(server_addr_and_port, &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel with " << server_addr_and_port;
    return false;
  }
  return true;
}

bool EtcdClient::Init(const char* server_addr_and_port) {
  brpc::ChannelOptions options;
  options.protocol = "http";
  if (_channel.Init(server_addr_and_port, &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel with " << server_addr_and_port;
    return false;
  }
  return true;
}

bool EtcdClient::Init(const char* server_addr, int port) {
  brpc::ChannelOptions options;
  options.protocol = "http";
  if (_channel.Init(server_addr, port, &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel with " << server_addr
        << ":" << port;
    return false;
  }
  return true;
}
*/

bool EtcdClient::Init(const std::vector<std::pair<std::string, int>>& ip_ports,
                      const std::string& lb) {
  if (ip_ports.size() <= 0) {
    LOG(ERROR) << "Init urls is empty.";
    return false;
  }
  _chan_size = ip_ports.size();
  LOG(INFO) << "Start Init EtcdClient, master node num:" << _chan_size;
  _chan_list.resize(_chan_size);
  brpc::ChannelOptions options;
  options.protocol = "http";
  for (int i = 0; i < _chan_size; ++i) {
    const auto& ip_port = ip_ports[i];
    _chan_list[i] = std::make_unique<brpc::Channel>();
    std::string url =
        "http://" + ip_port.first + ":" + std::to_string(ip_port.second);
    LOG(INFO) << "etcd one node url=[" << url << "]";
    if (_chan_list[i]->Init(url.c_str(), lb.c_str(), &options) != 0) {
      LOG(ERROR) << "Fail to initialize channel with url:" << url;
      _chan_list[i] = nullptr;
      return false;
    }
  }
  return true;
}

std::unique_ptr<brpc::Channel>& GetChannel(
    int& pre_idx, std::vector<std::unique_ptr<brpc::Channel>>& chans) {
  int chan_size = chans.size();
  if (pre_idx >= 0) {
    pre_idx = (pre_idx + 1) % chan_size;
    return chans[pre_idx];
  }
  pre_idx = GenRandomInt(1, chan_size) - 1;
  return chans[pre_idx];
}

enum EtcdOps {
  RANGE = 0,
  PUT,
  DELETE_RANGE,
  TXN,
  COMPACT,
  LEASE_GRANT,
  LEASE_REVOKE,
  LEASE_KEEPALIVE,
  LEASE_TTL,
  LEASE_LEASES,
};

template <typename Request, typename Response>
static bool EtcdOp(std::vector<std::unique_ptr<brpc::Channel>>& chans,
                   EtcdOps op, const Request& request, Response* response) {
  // invoker will make sure the op is valid
  std::string path = "";
  switch (op) {
    case RANGE:
      path = "/v3/kv/range";
      break;
    case PUT:
      path = "/v3/kv/put";
      break;
    case DELETE_RANGE:
      path = "/v3/kv/deleterange";
      break;
    case TXN:
      path = "/v3/kv/txn";
      break;
    case COMPACT:
      path = "/v3/kv/compaction";
      break;
    case LEASE_GRANT:
      path = "/v3/lease/grant";
      break;
    case LEASE_REVOKE:
      path = "/v3/lease/revoke";
      break;
    case LEASE_KEEPALIVE:
      path = "/v3/lease/keepalive";
      break;
    case LEASE_TTL:
      path = "/v3/lease/timetolive";
      break;
    case LEASE_LEASES:
      path = "/v3/lease/leases";
      break;
    default:
      LOG(ERROR) << "Invalid operation " << op << " with "
                 << request.ShortDebugString();
      return false;
  }
  int chan_size = chans.size();
  int pre_idx = -1;
  for (int i = 0; i < chan_size; ++i) {
    auto& ch = GetChannel(pre_idx, chans);
    Controller cntl;
    cntl.set_timeout_ms(1000);
    brpc::URI& uri = cntl.http_request().uri();
    uri.set_path(path);
    cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
    std::string output;
    struct json2pb::Pb2JsonOptions pb2json_option;
    pb2json_option.bytes_to_base64 = true;
    if (!json2pb::ProtoMessageToJson(request, &output, pb2json_option)) {
      LOG(ERROR) << "convert PutRequest " << request.ShortDebugString()
                 << " to json fail.";
      continue;
    }
    cntl.request_attachment().append(output);
    ch->CallMethod(NULL, &cntl, NULL, NULL, NULL);
    if (cntl.Failed()) {
      LOG(ERROR) << "request " << request.ShortDebugString()
                 << " to etcd server fail."
                 << "error message : " << cntl.ErrorText();
      continue;
    }
    struct json2pb::Json2PbOptions json2pb_option;
    // std::cout << "path:" << path << " request:" << output << " responst:" <<
    // cntl.response_attachment().to_string() << std::endl;
    json2pb_option.base64_to_bytes = true;
    if (!json2pb::JsonToProtoMessage(cntl.response_attachment().to_string(),
                                     response, json2pb_option)) {
      LOG(ERROR) << "Json to proto fail for "
                 << cntl.response_attachment().to_string();
      continue;
    }
    return true;
  }
  LOG(ERROR) << "EtcdOp() query Etcd api[" << path << "] is fail.";
  return false;
}

bool EtcdClient::Put(const ::etcdserverpb::PutRequest& request,
                     ::etcdserverpb::PutResponse* response) {
  return EtcdOp(_chan_list, EtcdOps::PUT, request, response);
}

bool EtcdClient::Range(const ::etcdserverpb::RangeRequest& request,
                       ::etcdserverpb::RangeResponse* response) {
  // LOG(INFO) << "Range Request: " << request.DebugString();
  return EtcdOp(_chan_list, EtcdOps::RANGE, request, response);
}

bool EtcdClient::DeleteRange(const ::etcdserverpb::DeleteRangeRequest& request,
                             ::etcdserverpb::DeleteRangeResponse* response) {
  return EtcdOp(_chan_list, DELETE_RANGE, request, response);
}

bool EtcdClient::Txn(const ::etcdserverpb::TxnRequest& request,
                     ::etcdserverpb::TxnResponse* response) {
  return EtcdOp(_chan_list, EtcdOps::TXN, request, response);
}

bool EtcdClient::Compact(const ::etcdserverpb::CompactionRequest& request,
                         ::etcdserverpb::CompactionResponse* response) {
  return EtcdOp(_chan_list, EtcdOps::COMPACT, request, response);
}

bool EtcdClient::LeaseGrant(const ::etcdserverpb::LeaseGrantRequest& request,
                            ::etcdserverpb::LeaseGrantResponse* response) {
  return EtcdOp(_chan_list, EtcdOps::LEASE_GRANT, request, response);
}

bool EtcdClient::LeaseRevoke(const ::etcdserverpb::LeaseRevokeRequest& request,
                             ::etcdserverpb::LeaseRevokeResponse* response) {
  return EtcdOp(_chan_list, EtcdOps::LEASE_REVOKE, request, response);
}

bool EtcdClient::LeaseTimeToLive(
    const ::etcdserverpb::LeaseTimeToLiveRequest& request,
    ::etcdserverpb::LeaseTimeToLiveResponse* response) {
  return EtcdOp(_chan_list, EtcdOps::LEASE_TTL, request, response);
}

bool EtcdClient::LeaseLeases(const ::etcdserverpb::LeaseLeasesRequest& request,
                             ::etcdserverpb::LeaseLeasesResponse* response) {
  return EtcdOp(_chan_list, EtcdOps::LEASE_LEASES, request, response);
}

bool EtcdClient::LeaseKeepAlive(
    const ::etcdserverpb::LeaseKeepAliveRequest& request,
    ::etcdserverpb::HttpLeaseKeepAliveResponse* response) {
  // LOG(INFO) << "LeaseKeepAliveResponse Request: " << request.DebugString();
  return EtcdOp(_chan_list, EtcdOps::LEASE_KEEPALIVE, request, response);
}

class ReadBody : public brpc::ProgressiveReader, public brpc::SharedObject {
 public:
  ReadBody() : _destroyed(false) {
    butil::intrusive_ptr<ReadBody>(this).detach();
  }

  butil::Status OnReadOnePart(const void* data, size_t length) {
    if (length > 0) {
      butil::IOBuf os;
      os.append(data, length);
      ::etcdserverpb::WatchResponseEx response;
      struct json2pb::Json2PbOptions option;
      option.base64_to_bytes = true;
      option.allow_remaining_bytes_after_parsing = true;
      std::string err;
      if (!json2pb::JsonToProtoMessage(os.to_string(), &response, option,
                                       &err)) {
        _watcher->OnParseError();
        LOG(WARNING) << "watch parse " << os.to_string() << " fail. "
                     << "error msg " << err;
      } else {
        _watcher->OnEventResponse(response.result());
      }
    }
    return butil::Status::OK();
  }

  void OnEndOfMessage(const butil::Status& st) {
    butil::intrusive_ptr<ReadBody>(this, false);
    _destroyed = true;
    _destroying_st = st;
    return;
  }

  bool destroyed() const { return _destroyed; }

  const butil::Status& destroying_status() const { return _destroying_st; }

  void set_watcher(std::shared_ptr<Watcher> watcher) { _watcher = watcher; }

 private:
  bool _destroyed;
  butil::Status _destroying_st;
  std::shared_ptr<Watcher> _watcher;
};

bool EtcdClient::Watch(const ::etcdserverpb::WatchRequest& request,
                       WatcherPtr watcher) {
  int pre_idx = -1;
  for (int i = 0; i < _chan_size; ++i) {
    auto& ch = GetChannel(pre_idx, _chan_list);
    if (!watcher.get()) {
      LOG(ERROR) << "watcher is nullptr";
      return false;
    }
    brpc::Controller cntl;
    cntl.http_request().uri().set_path("/v3/watch");
    cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
    std::string output;
    struct json2pb::Pb2JsonOptions pb2json_option;
    pb2json_option.bytes_to_base64 = true;
    if (!json2pb::ProtoMessageToJson(request, &output, pb2json_option)) {
      LOG(ERROR) << "convert WatchRequest " << request.ShortDebugString()
                 << " to json fail.";
      return false;
    }
    cntl.request_attachment().append(output);
    cntl.response_will_be_read_progressively();
    ch->CallMethod(NULL, &cntl, NULL, NULL, NULL);
    if (cntl.Failed()) {
      LOG(ERROR) << "watch request " << request.ShortDebugString()
                 << " to etcd server fail."
                 << "error message : " << cntl.ErrorText();
      return false;
    }
    butil::intrusive_ptr<ReadBody> reader;
    reader.reset(new ReadBody);
    reader->set_watcher(watcher);
    cntl.ReadProgressiveAttachmentBy(reader.get());
    return true;
  }
  return false;
}

void* EtcdClient::KeepNodeAliveThreadFunc(void* args) {
  auto register_func = [](brpc::EtcdClient* cli, std::string key,
                          std::string val, int ttl) {
    LOG(INFO) << "Begin register grant and key[" << key << "] and value[" << val
              << "]";
    if (cli == nullptr || key.empty()) {
      LOG(ERROR) << "client is nullptr or key is empty";
      return (int64_t)-1;
    }

    ::etcdserverpb::LeaseGrantRequest grant_req;
    ::etcdserverpb::LeaseGrantResponse grant_resp;
    grant_req.set_ttl(ttl);
    // LOG(INFO) << "LeaseGrant Request: " << grant_req.DebugString();
    cli->LeaseGrant(grant_req, &grant_resp);
    // LOG(INFO) << "LeaseGrant response: " << grant_resp.DebugString();
    ::etcdserverpb::PutRequest put_req;
    ::etcdserverpb::PutResponse put_resp;
    put_req.set_key(key);
    put_req.set_lease(grant_resp.id());
    put_req.set_value(val);
    // LOG(INFO) << "Put Request: " << put_req.DebugString();
    cli->Put(put_req, &put_resp);
    // LOG(INFO) << "Put response: " << put_resp.DebugString();
    return grant_resp.id();
  };

  KeepAliveArgs* ka = (KeepAliveArgs*)args;
  int64_t grant_id =
      register_func(ka->etcd_cli_, ka->key_, ka->val_, 3 * ka->interval_sec_);

  // ::etcdserverpb::RangeRequest range_req;
  // range_req.set_key(ka->key_);

  ::etcdserverpb::LeaseKeepAliveRequest alive_req;
  alive_req.set_id(grant_id);
  while (ka->etcd_cli_->_keep_alive_bt_running) {
    ::etcdserverpb::HttpLeaseKeepAliveResponse alive_resp;
    // ::etcdserverpb::RangeResponse range_resp;
    // ka->etcd_cli_->Range(range_req, &range_resp);
    if (ka->etcd_cli_->LeaseKeepAlive(alive_req, &alive_resp) == false ||
        alive_resp.result().ttl() <= 0) {
      // LOG(INFO) << "lease grant_id:[" << grant_id << "] expired or revoked,
      // re-register node";
      grant_id = register_func(ka->etcd_cli_, ka->key_, ka->val_,
                               3 * ka->interval_sec_);
      alive_req.set_id(grant_id);
    }
    // std::cout << "LeaseKeepAlive response: " << alive_resp.DebugString() <<
    // std::endl;
    sleep(ka->interval_sec_);
  }
  delete ka;
  return nullptr;
}

int EtcdClient::RegisterNodeWithKeepAlive(const std::string& node_key,
                                          const std::string& node_val,
                                          int interval_second) {
  LOG(INFO) << "**** start register node, interval_second:" << interval_second
            << " *****";
  this->_keep_alive_bt_running = true;
  KeepAliveArgs* thread_args =
      new KeepAliveArgs{this, node_key, node_val, interval_second};
  int ret = bthread_start_background(&_keep_alive_bt, nullptr,
                                     EtcdClient::KeepNodeAliveThreadFunc,
                                     thread_args);
  if (ret != 0) {
    std::cerr << "Failed to create bthread. Error code: " << ret << std::endl;
    return -1;
  }
  return 0;
}

std::string GetRangeEnd(const std::string& key) {
  std::string range_end = key;
  if (key.size() > 0) {
    range_end.back() += 1;
  }
  return range_end;
}

}  // namespace brpc