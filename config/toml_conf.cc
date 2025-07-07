

#include "toml_conf.h"
#include "third_party/toml11/include/toml.hpp"
#include "utils/log.h"

std::string TomlConfig::Load(const std::string& filepath) {
    toml::value toml_data;
    try {
        toml_data = toml::parse(filepath);
    } catch (const toml::syntax_error& err) {
        LOG(ERROR) << "Syntax error in TOML file: " << err.what() << std::endl;
        return "syntax_error";
    } catch (const std::runtime_error& err) {
        LOG(ERROR) << "Failed to parse TOML: " << err.what() << std::endl;
        return "runtime_error";
    }
    try {
        // Global
        auto toml_global = toml::find<toml::value>(toml_data, "global");
        global.name = toml::find<std::string>(toml_global, "name");
        global.data = toml::find<std::vector<std::string>>(toml_global, "data");
        global.level = toml::find<std::string>(toml_global, "level");
        global.signkey = toml::find<std::string>(toml_global, "signkey");
        global.skip_auth = toml::find<bool>(toml_global, "skip_auth");

        // Masters
        for (const auto& m : toml::find<std::vector<toml::value>>(toml_data, "masters")) {
            MasterConfig master;
            master.name = toml::find<std::string>(m, "name");
            master.address = toml::find<std::string>(m, "address");
            master.api_port = toml::find<int>(m, "api_port");
            master.etcd_port = toml::find<int>(m, "etcd_port");
            master.etcd_peer_port = toml::find<int>(m, "etcd_peer_port");
            master.etcd_client_port = toml::find<int>(m, "etcd_client_port");
            masters.push_back(master);
        }

        // Router
        auto toml_router = toml::find<toml::value>(toml_data, "router");
        router.port = toml::find<int>(toml_router, "port");
        router.skip_auth = toml::find<bool>(toml_router, "skip_auth");
        router.hearbeat_interval_s = toml::find<int>(toml_router, "hearbeat_interval_s");

        // PS
        auto toml_ps = toml::find<toml::value>(toml_data, "ps");
        ps.rpc_port = toml::find<int>(toml_ps, "rpc_port");
        ps.raft_election_timeout_ms = toml::find<int>(toml_ps, "raft_election_timeout_ms");
        ps.raft_snap_interval_s = toml::find<int>(toml_ps, "raft_snap_interval_s");
        ps.hearbeat_interval_s = toml::find<int>(toml_ps, "hearbeat_interval_s");
    } catch (const toml::type_error& err) {
        LOG(ERROR) << "TOML type error: " << err.what() << std::endl;
        return "type_error";
    } catch (const toml::exception& err) {
        LOG(ERROR) << "Missing key in TOML: " << err.what() << std::endl;
        return "missing_key";
    }
    for (const auto& m : masters) {
        std::string url = m.address + ":" + std::to_string(m.api_port);
        master_urls.push_back(url);
    }
    LOG(INFO) << "toml parse result:" << to_string();
    return "";
}

std::vector<std::pair<std::string, int>> TomlConfig::GetMasterIpPorts() {
    std::vector<std::pair<std::string, int>> res;
    for(const auto& m : masters) {
        res.push_back({m.address, m.etcd_client_port});
    }
    return res;
}
