#pragma once

#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

inline std::string listify(std::vector<const char*> vs) {
    std::string r;
    for (const auto& v : vs) {
        r += v + std::string(", ");
    }
    if (r.length() == 0) return "";
    return r.substr(0, r.length ()- 2);
}
