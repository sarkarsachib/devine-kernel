# HyperMonolithic Kernel Framework

This is the core scaffolding for the HyperMonolithic architecture. It provides a modular monolithic design with a central kernel core, subsystems, service locator, and lifecycle management.

## Architecture

The architecture revolves around the `kernel::Kernel` class which manages the lifecycle of `kernel::ISubsystem` modules. Subsystems are automatically registered using macros and can communicate via a type-safe Service Locator.

### Key Components

*   **Kernel Core**: Manages initialization, shutdown, and the service registry.
*   **Subsystems**: Modular components (e.g., Memory Manager, Scheduler, Drivers) that implement the `ISubsystem` interface.
*   **Service Locator**: Allows subsystems to publish and retrieve services (interfaces) safely.
*   **Security**: Tracks privilege levels (`User`, `Driver`, `Kernel`) per subsystem.
*   **Logging & Assertions**: Standardized logging and panic handling.

## Getting Started

### implementation a Subsystem

To create a new subsystem, inherit from `kernel::ISubsystem` and implement the required methods.

```cpp
#include "kernel/Subsystem.hpp"
#include "kernel/Log.hpp"
#include "kernel/Registry.hpp"

class MySubsystem : public kernel::ISubsystem {
public:
    const char* name() const override { return "MySubsystem"; }
    
    // Optional: Define privilege level (default is User)
    kernel::PrivilegeLevel get_privilege_level() const override { 
        return kernel::PrivilegeLevel::Driver; 
    }

    void on_early_init(kernel::Kernel& kernel) override {
        kernel::log(kernel::LogLevel::INFO, name(), "Early Init");
    }

    void on_late_init(kernel::Kernel& kernel) override {
        // Can resolve services here
    }
    
    void on_shutdown(kernel::Kernel& kernel) override {
        kernel::log(kernel::LogLevel::INFO, name(), "Shutdown");
    }
};

// Register the subsystem so the Kernel picks it up automatically
REGISTER_SUBSYSTEM(MySubsystem);
```

### Using the Service Locator

Subsystems can register services (pointers to interfaces or objects) and retrieve them.

**Registering a Service:**

```cpp
// inside a subsystem
static MyServiceImpl my_service_instance;
kernel.register_service<IMyService>(&my_service_instance);
```

**Retrieving a Service:**

```cpp
// inside another subsystem
IMyService* service = kernel.get_service<IMyService>();
if (service) {
    service->do_something();
}
```

### Logging and Assertions

Use `kernel::log` for logging and `K_ASSERT` for assertions.

```cpp
#include "kernel/Log.hpp"
#include "kernel/Assert.hpp"

kernel::log(kernel::LogLevel::INFO, "Module", "Message");

K_ASSERT(ptr != nullptr, "Pointer must not be null");
```

## Build System

The project uses CMake.

```bash
mkdir build
cd build
cmake ..
make
```

## Directory Structure

*   `include/kernel/`: Public headers (`Kernel.hpp`, `Subsystem.hpp`, `Log.hpp`, etc.)
*   `src/kernel/`: Implementation files
*   `src/main.cpp`: Example usage
