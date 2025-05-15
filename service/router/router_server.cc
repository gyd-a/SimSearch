
#include <string>
#include <unistd.h>
#include <brpc/server.h>          // brpc::Server

#include "service/router/router_server.h"
#include "config/conf.h"


bool RouterServer::Start() {
  std::string restful_mappings =
      "space/add_space => AddSpace, "
      "space/list => SpaceList";
      "space/delete_space => DeleteSpace";
  if (_server.AddService(&_space_impl, brpc::SERVER_DOESNT_OWN_SERVICE,
                         restful_mappings) != 0) {
    LOG(ERROR) << "Fail to add RouterSpaceRpcImpl service";
    return -1;
  }
  restful_mappings =
      "/document/get => Get, "
      "/document/search => Search, "
      "/document/upsert => Upsert, "
      "/document/delete => Delete, "
      "/document/mock/upsert => MockUpsert, "
      "/document/mock/get => MockGet, ";
  if (_server.AddService(&_doc_impl, brpc::SERVER_DOESNT_OWN_SERVICE,
                         restful_mappings) != 0) {
    LOG(ERROR) << "Fail to add RouterDocRpcImpl service";
    return -1;
  }
  if (_server.Start(toml_conf_.router.port, NULL) != 0) {
    LOG(ERROR) << "Fail to start RouterRpcImpl Server";
    return -1;
  }
  LOG(INFO) << "RouterRpcImpl service is running on "
            << _server.listen_address();
  return 0;
}

void RouterServer::WaitBrpcStop() {
  // Wait until 'CTRL-C' is pressed. then Stop() and Join() the service
  while (!brpc::IsAskedToQuit()) {
    sleep(1);
  }
  LOG(INFO) << "RouterServer service is going to quit";

  // Stop block before server
  _server.Stop(0);

  // Wait until all the processing tasks are over.
  _server.Join();
  LOG(INFO) << "RouterServer complete join";
}
