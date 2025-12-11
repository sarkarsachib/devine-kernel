use crate::process::elf_loader::TargetArch;

struct UserspaceEntry {
    path: &'static str,
    x86_64: &'static [u8],
    aarch64: Option<&'static [u8]>,
}

// Prebuilt ELF files - these would normally be compiled and included
// For now, we provide empty placeholders to allow the kernel to compile
// In a production system, these would be actual ELF binaries

// Create placeholder data for each program
const PLACEHOLDER: &[u8] = &[];

static PROGRAMS: &[UserspaceEntry] = &[
    UserspaceEntry {
        path: "/sbin/init",
        x86_64: PLACEHOLDER,
        aarch64: Some(PLACEHOLDER),
    },
    UserspaceEntry {
        path: "/bin/sh",
        x86_64: PLACEHOLDER,
        aarch64: Some(PLACEHOLDER),
    },
    UserspaceEntry {
        path: "/bin/cat",
        x86_64: PLACEHOLDER,
        aarch64: Some(PLACEHOLDER),
    },
    UserspaceEntry {
        path: "/bin/ls",
        x86_64: PLACEHOLDER,
        aarch64: Some(PLACEHOLDER),
    },
    UserspaceEntry {
        path: "/usr/bin/stress",
        x86_64: PLACEHOLDER,
        aarch64: Some(PLACEHOLDER),
    },
];

pub fn lookup(path: &str, arch: TargetArch) -> Option<&'static [u8]> {
    let entry = PROGRAMS.iter().find(|entry| entry.path == path)?;
    match arch {
        TargetArch::X86_64 => Some(entry.x86_64),
        TargetArch::AArch64 => entry.aarch64,
    }
}
