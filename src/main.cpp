#include "Filesystem.h"
#include "FileTree.h"
#include "Version.h"
#include "Connection.h"
#include "Config.h"
#include "StateController.h"
#include "Terminal.h"

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

using namespace fmerge;


namespace fmerge {
    bool debug_protocol{false};
}


int server_mode(std::string path) {
    termbuf() << "Starting in server mode for \"" << path << "\"" << std::endl;

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

    termbuf() << "Waiting for peer connections..." << std::endl;

    // Wait for peers
    listen_for_peers(4512, [=, &config](auto conn) {
        termbuf() << "Accepted connection from " << conn->get_address() << std::endl;

        StateController controller(std::move(conn), path, config);
        controller.run();
    });

    return 0;
}


int client_mode(std::string path, std::string target_address) {
    termbuf() << "Starting in client mode for \"" << path << "\"" << std::endl;

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
    connect_to_server(4512, target_address, [=, &config](auto conn) {
        termbuf() << "Connected to " << conn->get_address() << std::endl;
        
        StateController controller(std::move(conn), path, config);
        controller.run();
    });

    return 0;
}


void atexit_handler() {
    kill_term();
}


static struct option long_options[] {
    {"server", no_argument, 0, 's'},
    {"client", required_argument, 0, 0},
    {0, 0, 0, 0},
};


int main(int argc, char* argv[]) {
    // Register exit handlers
    if(std::atexit(atexit_handler)) {
        std::cerr << "main: failed to register exit handler!" << std::endl;
        return 1;
    }

    // Parse command line options
    int long_option_index{};
    int opt{};

    // Collection of flags to populate
    int mode = -1; // 0: server, 1: client
    std::string target_address{};
    std::string path_opt{};

    while((opt = getopt_long(argc, argv, "sc:vd", long_options, &long_option_index)) != -1) {
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
        } else if(opt == 'd') {
            debug_protocol = true;
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

    // Not using termbuf prevents an extra newline from being inserted
    std::cout << "Usage: fmerge (-s|-c server_ip) [path]" << std::flush;
    
    return 1;
}