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


    static void _handle_int(int) {}

    void register_trivial_sigint() {
        struct sigaction int_handler;
        sigemptyset(&int_handler.sa_mask);
        int_handler.sa_flags = 0;
        int_handler.sa_handler = _handle_int;
        if(sigaction(SIGINT, &int_handler, 0)) {
            print_clib_error("sigaction");
        }
    }


    std::string make_centered(const std::string& contents, int width, char padding_char) {
        auto inner_width = static_cast<int>(contents.length());

        // Amount of padding to put on left and right
        int padding_l = 0;
        int padding_r = 0;
        if(inner_width < width - 3) {
            padding_l = (width - inner_width) / 2 - 1;
            padding_r = width - inner_width - padding_l - 2;
        }

        return std::string(padding_l, padding_char) + " " + contents + " " + std::string(padding_r, padding_char);
    }
    
}