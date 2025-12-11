#include "kernel/Log.hpp"
#include <iostream>
#include <cstdlib>

namespace kernel {

    void log(LogLevel level, std::string_view module, std::string_view message) {
        const char* levelStr = "UNKNOWN";
        switch (level) {
            case LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LogLevel::INFO: levelStr = "INFO"; break;
            case LogLevel::WARNING: levelStr = "WARNING"; break;
            case LogLevel::ERROR: levelStr = "ERROR"; break;
            case LogLevel::CRITICAL: levelStr = "CRITICAL"; break;
        }
        std::cout << "[" << levelStr << "] [" << module << "] " << message << std::endl;
    }

    [[noreturn]] void panic(std::string_view message) {
        std::cerr << "KERNEL PANIC: " << message << std::endl;
        std::abort();
    }
}
