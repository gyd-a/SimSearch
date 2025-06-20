#pragma once

#include <idl/gen_idl/rpc_service_idl/common_rpc.pb.h>
#include <idl/gen_idl/rpc_service_idl/master_rpc.pb.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "clients/etcd_client.h"
#include "clients/ps_rpc_client.h"
#include "raft_store/raft_client.h"
#include "service/router/local_spaces.h"
#include "service/router/space_connection.h"
#include "utils/one_writer_multi_reader_map.h"
#include "utils/random.h"

class SpaceManager {
 public:
  std::string Init(int16_t max_space_num, brpc::EtcdClient* etcd_client);

  std::string AddSpace(const common_rpc::Space& pb_space);

  std::string DeleteSpace(const std::string& space_key);

  std::shared_ptr<SpaceConnection> GetSpaceConn(const std::string& space_key,
                                                std::string& msg);

  std::map<std::string, common_rpc::MockDoc> GetDocsFromPs(
      const std::string& space_key, const std::vector<std::string>& docs_key,
      std::string& msg);

  std::map<std::string, std::string> UpsertDocsToPs(
      const std::string& space_key, const std::map<int, DocsGroup>& docs_groups,
      std::string& msg, int test_id);

  std::map<int, DocsGroup> SplitDocs(
      const google::protobuf::RepeatedPtrField<common_rpc::MockDoc>& docs,
      int partition_num);

  static std::string SpaceJsonToPb(const std::string& space_json,
                                   common_rpc::Space& pb_space);

  static SpaceManager& GetInstance();

 private:
  SpaceManager() : _space_idx_mp(500) {}
  SpaceManager(const SpaceManager&) = delete;
  SpaceManager(SpaceManager&&) = delete;
  SpaceManager& operator=(const SpaceManager&) = delete;
  SpaceManager& operator=(SpaceManager&&) = delete;

  std::vector<std::shared_ptr<SpaceConnection>> _spaces_conn;
  OneWriterMultiReaderMap _space_idx_mp;
  brpc::EtcdClient* _etcd_client;
  std::mutex _mtx;
};
