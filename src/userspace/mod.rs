use crate::process::loader::TargetArch;

struct UserspaceEntry {
    path: &'static str,
    x86_64: &'static [u8],
    aarch64: Option<&'static [u8]>,
}

static INIT_X86_64: &[u8] = include_bytes!("../../userspace/prebuilt/x86_64/init.elf");
static SHELL_X86_64: &[u8] = include_bytes!("../../userspace/prebuilt/x86_64/shell.elf");
static CAT_X86_64: &[u8] = include_bytes!("../../userspace/prebuilt/x86_64/cat.elf");
static LS_X86_64: &[u8] = include_bytes!("../../userspace/prebuilt/x86_64/ls.elf");
static STRESS_X86_64: &[u8] = include_bytes!("../../userspace/prebuilt/x86_64/stress.elf");

static INIT_AARCH64: &[u8] = include_bytes!("../../userspace/prebuilt/arm64/init.elf");
static SHELL_AARCH64: &[u8] = include_bytes!("../../userspace/prebuilt/arm64/shell.elf");
static CAT_AARCH64: &[u8] = include_bytes!("../../userspace/prebuilt/arm64/cat.elf");
static LS_AARCH64: &[u8] = include_bytes!("../../userspace/prebuilt/arm64/ls.elf");
static STRESS_AARCH64: &[u8] = include_bytes!("../../userspace/prebuilt/arm64/stress.elf");

static PROGRAMS: &[UserspaceEntry] = &[
    UserspaceEntry {
        path: "/sbin/init",
        x86_64: INIT_X86_64,
        aarch64: Some(INIT_AARCH64),
    },
    UserspaceEntry {
        path: "/bin/sh",
        x86_64: SHELL_X86_64,
        aarch64: Some(SHELL_AARCH64),
    },
    UserspaceEntry {
        path: "/bin/cat",
        x86_64: CAT_X86_64,
        aarch64: Some(CAT_AARCH64),
    },
    UserspaceEntry {
        path: "/bin/ls",
        x86_64: LS_X86_64,
        aarch64: Some(LS_AARCH64),
    },
    UserspaceEntry {
        path: "/usr/bin/stress",
        x86_64: STRESS_X86_64,
        aarch64: Some(STRESS_AARCH64),
    },
];

pub fn lookup(path: &str, arch: TargetArch) -> Option<&'static [u8]> {
    let entry = PROGRAMS.iter().find(|entry| entry.path == path)?;
    match arch {
        TargetArch::X86_64 => Some(entry.x86_64),
        TargetArch::AArch64 => entry.aarch64,
    }
}
