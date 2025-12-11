#pragma once
#include <string_view>

namespace kernel {
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    void log(LogLevel level, std::string_view module, std::string_view message);
    
    [[noreturn]] void panic(std::string_view message);
}
