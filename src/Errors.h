#include <iostream>
#include <cstring>

namespace fmerge {

    inline void print_clib_error(std::string component) {
        std::cerr << component << ": " << strerror(errno) << " [code: " << errno << "]" << std::endl;
    };
    
}