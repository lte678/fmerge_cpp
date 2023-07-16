#pragma once

#include <json/json.hpp>

using json = nlohmann::json;

json load_config(std::string path);
void save_config(std::string path, const json &config);

int ensure_dir(std::string path);