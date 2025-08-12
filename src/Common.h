#pragma once

#include <vector>
#include <iostream>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i32 = std::int32_t;

#define LOG_INFO(s) std::cout << "[INFO] " << s << std::endl

struct FrameTimeInfo {
    // time between each frame in milliseconds
    double frameTime = 0;
    // time for each frame to be drawn in milliseconds
    double gpuTime = 0;
};

inline std::string listify(std::vector<const char*> vs) {
    std::string r;
    for (const auto& v : vs) {
        r += v + std::string(", ");
    }
    if (r.length() == 0) return "";
    return r.substr(0, r.length ()- 2);
}
