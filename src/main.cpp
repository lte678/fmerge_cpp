#include "Filesystem.h"
#include "FileTree.h"
#include "Version.h"
#include "Peer.h"
#include "Util.h"
#include "Config.h"

#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <csignal>
#include <fstream>
#include <iostream>


// TODOs:
//
// * Also use nsec component of mtime
// * 
//

std::shared_ptr<DirNode> build_tree(std::string path) {
    std::string filetree_file = join_path(path, ".fmerge/filetree.db");
    std::string filechanges_file = join_path(path, ".fmerge/filechanges.db");

    auto root_stats = get_file_stats(path);
    if(!root_stats.has_value()) {
        std::cerr << "Illegal starting folder" << std::endl;
        return nullptr;
    }
    MetadataNode root_metadata(split_path(path).back(), root_stats->mtime);
    auto root_node = std::make_shared<DirNode>(root_metadata);
    update_file_tree(root_node, path); // This is where the current file tree is built in memory

    // Attempt to detect changes
    auto database_file_stats = get_file_stats(filetree_file);
    if(!database_file_stats.has_value()) {
        std::cout << "[Warning] No historical filetree.db containing historical data found. Changes cannot be detected." << std::endl;
    } else {
        std::ifstream serialized_tree(filetree_file, std::ios_base::binary); 
        auto read_from_disk_node = DirNode::deserialize(serialized_tree);
        // Only check for changes if we were able to load historical data
        auto new_changes = compare_trees(read_from_disk_node, root_node);
        //for(const auto& change : new_changes) {
        //    std::cout << change << std::endl;
        //}

        // Read old change log
        std::vector<Change> all_changes{};
        if(get_file_stats(filechanges_file).has_value()) {
            std::ifstream serialized_changes(filechanges_file); 
            all_changes = read_changes(serialized_changes);
        }
        // Append new changes
        all_changes.insert(all_changes.end(), new_changes.begin(), new_changes.end());
        // Write new change log
        std::ofstream serialized_changes(filechanges_file, std::ios_base::trunc); 
        write_changes(serialized_changes, all_changes);
    }

    // Update the filetree.db with the latest state
    std::ofstream serialized_tree(filetree_file, std::ios_base::trunc | std::ios_base::binary); 
    root_node->serialize(serialized_tree);

    return root_node;
}


int server_mode(std::string path) {
    std::cout << "Starting in server mode for \"" << path << "\"" << std::endl;

    std::string config_dir = join_path(path, ".fmerge");
    std::string config_file = join_path(config_dir, "config.json");
    ensure_dir(config_dir);

    // Load config
    auto config = load_config(config_file);
    save_config(config_file, config);

    // Build file tree
    auto filetree = build_tree(path);

    // Wait for peers
    listen_for_peers(4512, config["uuid"], [](Peer client) {
        std::cout << "Accepted connection from peer with UUID ";
        print_uuid(std::cout, client.get_uuid());
        std::cout << std::endl;
    });

    return 0;
}


int client_mode(std::string path, std::string target_address) {
    std::cout << "Starting in client mode for \"" << path << "\"" << std::endl;

    std::string config_dir = join_path(path, ".fmerge");
    std::string config_file = join_path(config_dir, "config.json");
    ensure_dir(config_dir);

    // Load config
    auto config = load_config(config_file);
    save_config(config_file, config);

    // Build file tree
    auto filetree = build_tree(path);

    // Connect to server
    connect_to_server(4512, target_address, config["uuid"], [target_address](Peer server) {
        std::cout << "Connected to " << target_address << " with UUID ";
        print_uuid(std::cout, server.get_uuid());
        std::cout << std::endl;
    });

    return 0;
}


static struct option long_options[] {
    {"server", no_argument, 0, 's'},
    {"client", required_argument, 0, 0},
    {0, 0, 0, 0},
};


int main(int argc, char* argv[]) {
    int long_option_index{};
    int opt{};

    // Collection of flags to populate
    int mode = -1; // 0: server, 1: client
    std::string target_address{};
    std::string path_opt{};

    while((opt = getopt_long(argc, argv, "sc:v", long_options, &long_option_index)) != -1) {
        if(opt == 's') {
            if(mode != -1) {
                std::cerr << "Cannot set multiple server and/or client flags." << std::endl;
                return 1;
            }
            mode = 0;
        } else if(opt == 'c') {
            if(mode != -1) {
                std::cerr << "Cannot set multiple server and/or client flags." << std::endl;
                return 1;
            }
            mode = 1;
            target_address = optarg;
        } else if(opt == 'v') {
            std::cout << "Version " << MAJOR_VERSION << "." << MINOR_VERSION << std::endl;
            return 0;
        } else if(opt == '?') {
            // We got an invalid option
            if(optopt == 'c') {
                std::cout << "Usage: fmerge (-s|-c server_ip) [path]" << std::endl;
                return 1;
            }
            return 1;
        } else {
            std::cout << "getopt_long: " << static_cast<char>(opt) << std::endl;
        }
    }
    

    if(mode == 0 || mode == 1) {
        if(optind == (argc - 1)) {
            path_opt = argv[optind];
        } else if(optind == argc) {
            std::cout << "Missing path!" << std::endl;
            std::cout << "Usage: fmerge (-s|-c server_ip) [path]" << std::endl;
            return 1;
        } else {
            std::cout << "Only one path may be supplied." << std::endl;
            std::cout << "Usage: fmerge (-s|-c server_ip) [path]" << std::endl;
            return 1;
        }
    }

    if(mode == 0) {
        return server_mode(path_opt);
    } else if(mode == 1) {
        return client_mode(path_opt, target_address);
    }

    std::cout << "Usage: fmerge (-s|-c server_ip) [path]" << std::endl;
    return 1;
}