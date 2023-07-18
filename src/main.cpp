#include "Filesystem.h"
#include "FileTree.h"
#include "Version.h"
#include "Peer.h"
#include "Util.h"
#include "Config.h"

#include <unistd.h>
#include <getopt.h>
#include <csignal>
#include <iostream>
#include <fstream>


// TODOs:
//
// * Also use nsec component of mtime
// * 
//


int server_mode(std::string path) {
    std::cout << "Starting in server mode for \"" << path << "\"" << std::endl;

    if(!exists(path)) {
        std::cerr << "Illegal starting folder" << std::endl;
        return 1;
    }

    std::string config_dir = join_path(path, ".fmerge");
    std::string config_file = join_path(config_dir, "config.json");
    ensure_dir(config_dir);

    // Load config
    auto config = load_config(config_file);
    save_config(config_file, config);

    // Build file tree
    append_changes(path, get_new_tree_changes(path));

    // Wait for peers
    listen_for_peers(4512, config["uuid"], [](Peer client) {
        std::cout << "Accepted connection from peer with UUID " << to_string(client.get_uuid()) << std::endl;

        // TRANSACTION 2.1 <---- Receive relevant changes from remote
        auto msg_header = MessageHeader::deserialize(client.get_fd());
        if(msg_header.type != MsgSendChanges) {
            std::cerr << "Invalid message received during handshake (Received: " << msg_header.type << ")" << std::endl;
            return;
        }

        auto changes_msg = ChangesMessage::deserialize(client.get_fd(), msg_header.length);
        std::cout << "Received " << changes_msg.changes.size() << " changes" << std::endl;

        for(const auto& change : changes_msg.changes) {
            std::cout << change << std::endl;
        }
    });

    return 0;
}


int client_mode(std::string path, std::string target_address) {
    std::cout << "Starting in client mode for \"" << path << "\"" << std::endl;

    if(!exists(path)) {
        std::cerr << "Illegal starting folder" << std::endl;
        return 1;
    }

    std::string config_dir = join_path(path, ".fmerge");
    std::string config_file = join_path(config_dir, "config.json");
    ensure_dir(config_dir);

    // Load config
    auto config = load_config(config_file);
    save_config(config_file, config);

    // Build file tree
    append_changes(path, get_new_tree_changes(path));

    // Connect to server
    connect_to_server(4512, target_address, config["uuid"], 
    [=, &config](Peer server) {
        std::cout << "Connected to " << target_address << " with UUID " << to_string(server.get_uuid()) << std::endl;

        // Get the relevant changes
        std::vector<Change> new_remote_changes;
        
        // Get remote json config entry
        auto remote_config = get_remote_config(config, server.get_uuid());
        if(remote_config.has_value()) {
            // Remote that has connected before
            std::cout << "[Warning] Partial change lists have not been implemented yet!" << std::endl;
            std::ifstream change_file(join_path(config_dir, "filechanges.db"));
            new_remote_changes = read_changes(change_file);
        } else {
            // Remote that has never connected
            // Push all changes
            std::ifstream change_file(join_path(config_dir, "filechanges.db"));
            new_remote_changes = read_changes(change_file);
        }

        // TRANSACTION 2.1 ----> Send relevant changes to remote
        ChangesMessage changes_msg(new_remote_changes);
        changes_msg.serialize(server.get_fd());

        std::cout << "Transmitted " << new_remote_changes.size() << " changes" << std::endl;

        return 0;
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