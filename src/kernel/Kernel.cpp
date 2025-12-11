#include "kernel/Kernel.hpp"
#include "kernel/Log.hpp"

namespace kernel {

    Kernel::Kernel() {
        log(LogLevel::INFO, "Kernel", "Kernel initialized");
    }

    Kernel::~Kernel() {
        log(LogLevel::INFO, "Kernel", "Kernel destroyed");
    }

    void Kernel::early_init() {
        log(LogLevel::INFO, "Kernel", "Early Init Phase");
        for (auto& sub : subsystems_) {
            sub->on_early_init(*this);
        }
    }

    void Kernel::late_init() {
        log(LogLevel::INFO, "Kernel", "Late Init Phase");
        for (auto& sub : subsystems_) {
            sub->on_late_init(*this);
        }
    }

    void Kernel::shutdown() {
        log(LogLevel::INFO, "Kernel", "Shutdown Phase");
        // Shutdown in reverse order? Or same order? Usually reverse order of initialization.
        // Since subsystems are in a vector, we can iterate backwards.
        for (auto it = subsystems_.rbegin(); it != subsystems_.rend(); ++it) {
            (*it)->on_shutdown(*this);
        }
    }

    void Kernel::register_subsystem(std::unique_ptr<ISubsystem> subsystem) {
        log(LogLevel::DEBUG, "Kernel", "Registering subsystem: ");
        // Note: we can't easily log the name here if it's not constructed yet or if we just have the pointer.
        // Actually we have the unique_ptr so the object exists.
        log(LogLevel::DEBUG, "Kernel", subsystem->name());
        subsystems_.push_back(std::move(subsystem));
    }

}
