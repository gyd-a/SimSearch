#pragma once

#include <brpc/channel.h>

#include "utils/numeric_util.h"
#include <json2pb/json_to_pb.h>
#include <json2pb/pb_to_json.h>

#include "utils/log.h"

template <typename REQ, typename RESP>
int64_t HttpPost(const std::vector<std::string>& ip_port_list, const std::string& path,
                 const REQ& req, RESP& resp, int max_retry) {
  brpc::ChannelOptions options;
  options.protocol = "http";
  options.timeout_ms = 1000;  // 请求超时 ms
  options.connect_timeout_ms = 200;
  options.max_retry = 2;
  int addr_idx = GenRandomInt(0, ip_port_list.size() - 1);
  for (size_t i = 0; i < max_retry; ++i) {
    auto ip_port = ip_port_list[addr_idx];
    std::string log_prefix =
        std::string("HttpPost ip_port: ") + ip_port + " path: " + path;
    addr_idx = (addr_idx + 1) % ip_port_list.size();
    brpc::Channel channel;
    if (channel.Init(ip_port.c_str(), "", &options) != 0) {
      LOG(ERROR) << "Fail to initialize channel";
      continue;
    }

    brpc::Controller cntl;
    cntl.http_request().uri() = path;
    cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
    cntl.http_request().SetHeader("Content-Type", "application/json");

    std::string json_body;
    json2pb::Pb2JsonOptions req_options;
    req_options.bytes_to_base64 = true;  // 若你的 pb 有 bytes 字段建议开启
    json2pb::ProtoMessageToJson(req, &json_body, req_options);
    cntl.request_attachment().append(json_body);
    // 发起请求
    channel.CallMethod(NULL, &cntl, NULL, NULL, NULL);
    if (cntl.Failed()) {
      LOG(ERROR) << log_prefix << ", request failed: " << cntl.ErrorText()
                 << " request:" << json_body;
    } else {
      std::string resp_str = cntl.response_attachment().to_string();
      json2pb::Json2PbOptions resp_options;
      resp_options.base64_to_bytes = true;  // proto有bytes类型字段建议打开
      if (json2pb::JsonToProtoMessage(resp_str, &resp, resp_options)) {
        LOG(INFO) << log_prefix << " success!!!";
        return 0;
      } else {
        LOG(ERROR) << log_prefix << ", Failed to parse response: " << resp_str;
      }
    }
  }
  LOG(ERROR) << "HttpPost Failed, max_retry:" << max_retry << ", path:" << path;
  return -1;
}
