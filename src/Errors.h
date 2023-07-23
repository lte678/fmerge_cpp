#include <iostream>
#include <cstring>

namespace fmerge {

    // This flag signifies to print debug messages for the protocol.
    extern bool debug_protocol;

    inline void print_clib_error(std::string component) {
        std::cerr << component << ": " << strerror(errno) << " [code: " << errno << "]" << std::endl;
    };
    
}