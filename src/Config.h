#pragma once

#include <optional>
#include <json/json.hpp>

using std::optional;
using json = nlohmann::json;

json generate_new_config();
json load_config(std::string path);
void save_config(std::string path, const json &config);

optional<json> get_remote_config(json config, std::array<unsigned char, 16> peer_uuid);