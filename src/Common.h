#pragma once

#include <algorithm>
#include <vector>
#include <array>
#include <fstream>

#include "Logger.h"

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i32 = std::int32_t;

constexpr u32 FRAME_SET_NUMBER = 0;
constexpr u32 MATERIAL_SET_NUMBER = 1;
constexpr u32 MODEL_SET_NUMBER = 2;

struct FrameTimeInfo {
    // time between each frame in milliseconds
    double frameTime = 0.0;
    // time for each frame to be drawn in milliseconds
    double gpuTime = 0.0;
    // time for update processing (CPU side) in milliseconds
    double cpuTime = 0.0;
    // time for each draw call to be written to
    double drawWriteTime = 0.0;
};

struct VRAMUsageInfo {
    size_t gpuTotal = 0;
    size_t gpuAvailable = 0;
    size_t gpuUsed = 0;

    size_t sharedTotal = 0;
    size_t sharedAvailable = 0;
    size_t sharedUsed = 0;
};

inline std::string listify(const std::vector<const char*>& vs) {
    std::string r;
    for (const auto& v : vs) {
        r += v + std::string(", ");
    }
    if (r.empty()) return "";
    return r.substr(0, r.length ()- 2);
}

inline std::string storageSizeToString(size_t bytes) {
    static constexpr std::array units = {"B", "KB", "MB", "GB", "TB"};

    auto n = static_cast<double>(bytes);
    int numDivisions = 0;
    while (n >= 1000) {
        n /= 1000.0f;
        ++numDivisions;
        if (numDivisions >= units.size()) {
            break;
        }
    }
    return std::format("{:.2f} {}", n, units[numDivisions]);
}

inline std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

inline std::string tolower(const std::string& str) {
    std::string cpy = str;
    std::ranges::transform(cpy.begin(), cpy.end(), cpy.begin(), [](unsigned char c) { return std::tolower(c); });
    return cpy;
}

class IUpdatable {
public:
    virtual ~IUpdatable() = default;
    virtual void update(float deltaTime) = 0;
};
