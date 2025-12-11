#include "kernel/Kernel.hpp"
#include "kernel/Registry.hpp"
#include "kernel/Log.hpp"
#include <iostream>

class MemorySubsystem : public kernel::ISubsystem {
public:
    const char* name() const override { return "MemoryManager"; }
    
    kernel::PrivilegeLevel get_privilege_level() const override { return kernel::PrivilegeLevel::Kernel; }

    void on_early_init(kernel::Kernel& kernel) override {
        kernel::log(kernel::LogLevel::INFO, name(), "Initializing Memory...");
        // Register itself as a service (dummy int for now)
        static int memory_service = 42;
        kernel.register_service<int>(&memory_service);
    }
};

REGISTER_SUBSYSTEM(MemorySubsystem);

class SchedulerSubsystem : public kernel::ISubsystem {
public:
    const char* name() const override { return "Scheduler"; }
    
    void on_late_init(kernel::Kernel& kernel) override {
        kernel::log(kernel::LogLevel::INFO, name(), "Starting Scheduler...");
        int* mem = kernel.get_service<int>();
        if (mem) {
             kernel::log(kernel::LogLevel::INFO, name(), "Found Memory Service!");
        } else {
             kernel::log(kernel::LogLevel::ERROR, name(), "Memory Service NOT Found!");
        }
    }
};

REGISTER_SUBSYSTEM(SchedulerSubsystem);

int main() {
    kernel::Kernel k;
    
    // Load subsystems from registry
    auto& registry = kernel::SubsystemRegistry::instance();
    for (auto& factory : registry.factories()) {
        k.register_subsystem(factory());
    }

    k.early_init();
    k.late_init();
    
    // Run loop...
    
    k.shutdown();
    return 0;
}
