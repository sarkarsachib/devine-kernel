#include "kernel/Registry.hpp"

namespace kernel {

    SubsystemRegistry& SubsystemRegistry::instance() {
        static SubsystemRegistry instance;
        return instance;
    }

    void SubsystemRegistry::register_factory(SubsystemFactory factory) {
        factories_.push_back(factory);
    }

    const std::vector<SubsystemFactory>& SubsystemRegistry::factories() const {
        return factories_;
    }

    AutoRegister::AutoRegister(SubsystemFactory factory) {
        SubsystemRegistry::instance().register_factory(factory);
    }

}
