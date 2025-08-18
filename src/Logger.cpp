#include "Logger.h"

#include <iostream>

Logger Logger::s_instance = Logger();

Logger& Logger::get() {
    return s_instance;
}

void Logger::info(std::string s) {
    raw("[INFO] " + s);
}

void Logger::warn(std::string s) {
    raw("[WARN] " + s);
}

void Logger::error(std::string s) {
    throw std::runtime_error("[ERROR] " + s);
}

void Logger::raw(const std::string& s) {
    std::cout << s << std::endl;
}
