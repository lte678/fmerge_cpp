#pragma once

#include <iostream>
#include <cstring>

#include "Terminal.h"

namespace fmerge {
    inline void print_clib_error(std::string component) {
        LOG("[Error] " << component << ": " << strerror(errno) << " [code: " << errno << "]" << std::endl);
    };
    
}