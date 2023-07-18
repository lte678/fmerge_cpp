#include "Config.h"

#include "Filesystem.h"
#include "Errors.h"
#include "Util.h"

#include <fstream>
#include <uuid/uuid.h>


json generate_new_config() {
    uuid_t our_uuid;
    char our_uuid_str[37];
    uuid_generate_random(our_uuid);
    uuid_unparse(our_uuid, our_uuid_str);

    return json {
        {"uuid", our_uuid_str},
        {"remotes", json::array()}
    };
}


json load_config(std::string path) {
    if(get_file_stats(path).has_value()) {
        json config;
        std::ifstream json_file(path);
        json_file >> config;
        return config;
    } else {
        return generate_new_config();
    }
}


void save_config(std::string path, const json &config) {
    std::ofstream json_file(path);
    json_file << config;
}


optional<json> get_remote_config(json config, std::array<unsigned char, 16> peer_uuid) {
    for(const json& remote : config["remotes"]) {
        if(remote["uuid"] == to_string(peer_uuid)) {
            return remote;
        }
    }
    return std::nullopt;
}