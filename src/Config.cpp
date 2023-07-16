#include "Config.h"

#include "Filesystem.h"
#include "Errors.h"

#include <fstream>
#include <uuid/uuid.h>


json generate_new_config() {
    uuid_t our_uuid;
    char our_uuid_str[37];
    uuid_generate_random(our_uuid);
    uuid_unparse(our_uuid, our_uuid_str);

    return json {
        {"uuid", our_uuid_str}
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


int ensure_dir(std::string path) {
    // Create dir if it does not exist
    if(!get_file_stats(path).has_value()) {
        if(mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
            print_clib_error("mkdir");
            return 1;
        }
        std::cout << "Created " << split_path(path).back() << " directory" << std::endl;
    }
    return 0;
}