#pragma once
#include <string_view>

namespace kernel {

    class Kernel;

    enum class PrivilegeLevel {
        User,
        Driver,
        Kernel
    };

    class ISubsystem {
    public:
        virtual ~ISubsystem() = default;

        virtual const char* name() const = 0;
        virtual PrivilegeLevel get_privilege_level() const { return PrivilegeLevel::User; }

        // Lifecycle hooks
        virtual void on_early_init(Kernel& kernel) {}
        virtual void on_late_init(Kernel& kernel) {}
        virtual void on_shutdown(Kernel& kernel) {}
    };

}
