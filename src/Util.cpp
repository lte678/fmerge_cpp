#include "Util.h"

namespace fmerge {

    std::string to_string(std::array<unsigned char, 16> uuid) {
        std::stringstream out{};
        for(int i = 0; i < 4; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(uuid[i]);
        }
        out << "-";
        for(int i = 4; i < 6; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(uuid[i]);
        }
        out << "-";
        for(int i = 6; i < 8; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(uuid[i]);
        }
        out << "-";
        for(int i = 8; i < 10; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(uuid[i]);
        }
        out << "-";
        for(int i = 10; i < 16; i++) {
            out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(uuid[i]);
        }
        return out.str();
    }


    void register_trivial_sigint() {
        auto handle_int = [](int){};
        struct sigaction int_handler;
        sigemptyset(&int_handler.sa_mask);
        int_handler.sa_flags = 0;
        int_handler.sa_handler = handle_int;
        if(sigaction(SIGINT, &int_handler, 0)) {
            print_clib_error("sigaction");
        }
    }
    
}