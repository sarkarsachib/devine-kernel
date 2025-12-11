#pragma once
#include <vector>
#include <functional>
#include <memory>
#include "Subsystem.hpp"

namespace kernel {
    
    using SubsystemFactory = std::function<std::unique_ptr<ISubsystem>()>;

    class SubsystemRegistry {
    public:
        static SubsystemRegistry& instance();
        
        void register_factory(SubsystemFactory factory);
        const std::vector<SubsystemFactory>& factories() const;

    private:
        std::vector<SubsystemFactory> factories_;
    };

    struct AutoRegister {
        AutoRegister(SubsystemFactory factory);
    };
}

#define REGISTER_SUBSYSTEM(Type) \
    namespace { \
        static kernel::AutoRegister Type##_autoreg([]() { return std::make_unique<Type>(); }); \
    }
