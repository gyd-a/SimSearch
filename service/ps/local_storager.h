#pragma once

#include <string>
#include <vector>

#include "service/ps/local_mata.h"

class LocalStorager {
 public:
  ~LocalStorager();

  int64_t GetNodeIdByQueryApi(int max_retry);

  int64_t CreateOrLoadNodeMata(const std::string& dir_path);

  PsLocalNodeMata& PsNodeMata() { return _ps_node_mata; }

  std::pair<std::string, std::string> GenPsNodeKeyAndValue();

  std::string GetRaftRoot();

  bool GetRaftParams(std::string& root_path, std::string& group, std::string& conf,
                     int& port);

  std::string DeleteSpace(const std::string& db_name, const std::string& space_name);

  static LocalStorager& GetInstance();

 private:
  LocalStorager();
  LocalStorager(const LocalStorager&) = delete;
  LocalStorager(LocalStorager&&) = delete;
  LocalStorager& operator=(const LocalStorager&) = delete;
  LocalStorager& operator=(LocalStorager&&) = delete;

  // Other member functions and variables can be declared here
  PsLocalNodeMata _ps_node_mata;
  std::vector<std::string> _master_addrs;
};
