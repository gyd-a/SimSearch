#pragma once

#include <string>
#include <vector>

#include "config/toml_conf.h"

extern std::string _cur_IP;

extern std::string _etcd_space_prefix;
extern std::string _ps_register_prefix;        // + "{node_id}_{IP}"
extern std::string _router_register_prefix;  // + "{IP}:{PORT}"

extern std::string _data_dir;

extern std::string _router_store_dir;

extern std::string _ps_storage_dir;
extern std::string _ps_mata_file;
extern std::string _raft_root_dir;
extern std::string _engine_data_dir;
// router
extern TomlConfig toml_conf_;

extern std::string _log_dir;


int InitConfig(const std::string& ip, const std::string& ps_port,
               const std::string& router_port, const std::string& toml_file,
               const std::string& log_dir);

