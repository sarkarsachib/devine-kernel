fn main() {
    let target_arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    
    let mut build = cc::Build::new();
    build
        .file("src/cpp/crypto.cpp")
        .file("src/profiler.cpp")
        .cpp(true)
        .flag_if_supported("-std=c++17")
        .flag_if_supported("-fno-exceptions")
        .flag_if_supported("-fno-rtti")
        .flag_if_supported("-nostdlib")
        .flag_if_supported("-ffreestanding");

    if target_arch == "x86_64" {
        build
            .flag_if_supported("-mno-red-zone")
            .flag_if_supported("-mno-mmx")
            .flag_if_supported("-mno-sse");
    } else if target_arch == "aarch64" {
        build.flag_if_supported("-march=armv8-a");
    }

    build.compile("devine_perf");

    cxx_build::bridge("src/lib.rs")
        .file("src/cpp/crypto.cpp")
        .file("src/profiler.cpp")
        .flag_if_supported("-std=c++17")
        .flag_if_supported("-fno-exceptions")
        .flag_if_supported("-fno-rtti")
        .flag_if_supported("-nostdlib")
        .flag_if_supported("-ffreestanding")
        .compile("devine-perf-cpp");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=src/cpp/crypto.cpp");
    println!("cargo:rerun-if-changed=src/profiler.cpp");
    println!("cargo:rerun-if-changed=include/crypto.hpp");
    println!("cargo:rerun-if-changed=include/profiler.hpp");
}
