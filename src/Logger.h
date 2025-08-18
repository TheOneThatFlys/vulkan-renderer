#pragma once

#include <format>

class Logger {
public:
    static Logger& get();
    static void info(std::string s);
    static void warn(std::string s);
    static void error(std::string s);
    static void raw(const std::string& s);

private:
    Logger() {}

    static Logger s_instance;
};
