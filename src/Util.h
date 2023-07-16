#pragma once

#include <array>
#include <ostream>
#include <iomanip>


inline void print_uuid(std::ostream& out, std::array<unsigned char, 16> uuid) {
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
}