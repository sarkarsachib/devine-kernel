#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <string_view>
#include "Subsystem.hpp"
#include "Log.hpp"

namespace kernel {

    class Kernel {
    public:
        Kernel();
        ~Kernel();

        void early_init();
        void late_init();
        void shutdown();

        void register_subsystem(std::unique_ptr<ISubsystem> subsystem);

        // Service Locator
        template <typename T>
        void register_service(T* service) {
             services_[std::type_index(typeid(T))] = service;
             log(LogLevel::DEBUG, "Kernel", "Service registered");
        }

        template <typename T>
        T* get_service() {
            auto it = services_.find(std::type_index(typeid(T)));
            if (it != services_.end()) {
                return static_cast<T*>(it->second);
            }
            return nullptr;
        }

    private:
        std::vector<std::unique_ptr<ISubsystem>> subsystems_;
        std::unordered_map<std::type_index, void*> services_;
    };
}
