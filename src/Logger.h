#pragma once

#include <format>
#include <iostream>
#include <string>

class Logger {
public:
    Logger() = delete;

    template <typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        Logger::info(std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    static void info(const std::string &s) {
        raw("[INFO] " + s);
    }

    template <typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        Logger::warn(std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    static void warn(const std::string &s) {
        raw("[WARN] " + s);
    }

    template <typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        Logger::error(std::vformat(fmt.get(), std::make_format_args(args...)));
    }

    static void error(const std::string &s) {
        throw std::runtime_error("[ERROR] " + s);
    }

    static void raw(const std::string& s) {
        std::cout << s << std::endl;
    }
};
