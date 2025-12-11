use super::Process;

pub use super::elf_loader::{
    load_executable,
    AuxEntry,
    ElfLoaderError,
    LoadedImage,
    Segment,
    StackImage,
    TargetArch,
};

pub fn exec_into_process(
    process: &mut Process,
    image: &[u8],
    arch: TargetArch,
    argv: &[&str],
    envp: &[&str],
) -> Result<LoadedImage, ElfLoaderError> {
    let loaded = load_executable(image, arch, &process.address_space, argv, envp)?;
    process.image = Some(loaded.clone());
    Ok(loaded)
}
