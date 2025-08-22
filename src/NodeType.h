#pragma once
#include <format>
#include <string>

enum NodeType {
    eNode, eInstance, eScene
};

template <>
struct std::formatter<NodeType> : std::formatter<std::string> {
    auto format(NodeType p, std::format_context& ctx) const {
        const char *s;
        switch (p) {
            case eNode:
                s = "Node";
                break;
            case eInstance:
                s = "Instance";
                break;
            case eScene:
                s = "Scene";
                break;
            default:
                s = "UNKNOWN";
                break;
        }
        return formatter<std::string>::format(std::string(s), ctx);
    }
};