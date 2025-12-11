#[cfg(not(test))]
extern crate alloc;

use alloc::vec;
use alloc::vec::Vec;
use core::convert::TryInto;

use crate::memory::{VirtAddr, PAGE_SIZE};
use crate::memory::paging::PageFlags;
use super::AddressSpace;

const ELF_MAGIC: &[u8; 4] = b"\x7FELF";
const EI_CLASS: usize = 4;
const EI_DATA: usize = 5;
const EI_VERSION: usize = 6;
const ELFCLASS32: u8 = 1;
const ELFCLASS64: u8 = 2;
const ELFDATA2LSB: u8 = 1;
const PT_LOAD: u32 = 1;
const SHT_RELA: u32 = 4;
const SHT_REL: u32 = 9;
const PF_EXECUTE: u32 = 0x1;
const PF_WRITE: u32 = 0x2;
const PF_READ: u32 = 0x4;
const R_X86_64_RELATIVE: u32 = 8;
const R_AARCH64_RELATIVE: u32 = 1027;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TargetArch {
    X86_64,
    AArch64,
}

#[derive(Debug, Clone)]
pub struct LoadedImage {
    pub entry_point: VirtAddr,
    pub segments: Vec<Segment>,
    pub auxiliary: Vec<AuxEntry>,
    pub stack: StackImage,
    pub program_header_offset: usize,
    pub program_header_entry_size: usize,
    pub program_header_count: usize,
}

#[derive(Debug, Clone)]
pub struct Segment {
    pub vaddr: VirtAddr,
    pub mem_size: usize,
    pub file_size: usize,
    pub flags: PageFlags,
    pub data: Vec<u8>,
}

#[derive(Debug, Clone, Copy)]
pub struct AuxEntry {
    pub key: usize,
    pub value: usize,
}

#[derive(Debug, Clone)]
pub struct StackImage {
    pub user_sp: VirtAddr,
    pub stack_top: VirtAddr,
    pub stack_bottom: VirtAddr,
    pub data: Vec<u8>,
}

#[derive(Debug)]
pub enum ElfLoaderError {
    InvalidMagic,
    UnsupportedClass,
    UnsupportedEndianness,
    UnsupportedMachine,
    InvalidHeader,
    InvalidProgramHeader,
    UnexpectedEof,
    MissingLoadSegment,
    StackOverflow,
    RelocationUnsupported,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ElfClass {
    Elf32,
    Elf64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Endianness {
    Little,
    Big,
}

#[derive(Debug, Clone)]
struct ElfHeader {
    class: ElfClass,
    endian: Endianness,
    machine: u16,
    entry: u64,
    phoff: u64,
    shoff: u64,
    phentsize: u16,
    phnum: u16,
    shentsize: u16,
    shnum: u16,
}

#[derive(Debug, Clone)]
struct ProgramHeader {
    typ: u32,
    flags: u32,
    offset: u64,
    vaddr: u64,
    filesz: u64,
    memsz: u64,
    align: u64,
}

#[derive(Debug, Clone)]
struct SectionHeader {
    sh_type: u32,
    sh_offset: u64,
    sh_size: u64,
    sh_entsize: u64,
}

#[derive(Debug, Clone)]
struct Relocation {
    offset: u64,
    addend: i64,
    kind: RelocationKind,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum RelocationKind {
    X86Relative,
    AArch64Relative,
}

pub fn load_executable(
    image: &[u8],
    arch: TargetArch,
    address_space: &AddressSpace,
    argv: &[&str],
    envp: &[&str],
) -> Result<LoadedImage, ElfLoaderError> {
    let header = parse_header(image)?;
    validate_machine(&header, arch)?;

    let program_headers = parse_program_headers(image, &header)?;
    let mut segments = build_segments(image, &header, &program_headers)?;

    let sections = parse_section_headers(image, &header)?;
    let relocations = parse_relocations(image, &header, &sections, arch)?;
    apply_relocations(&mut segments, &relocations, arch, header.class)?;

    if segments.is_empty() {
        return Err(ElfLoaderError::MissingLoadSegment);
    }

    let aux = build_auxiliary(&header, argv.len(), envp.len());
    let stack = build_stack(address_space, argv, envp, &aux)?;

    Ok(LoadedImage {
        entry_point: VirtAddr::new(header.entry),
        segments,
        auxiliary: aux,
        stack,
        program_header_offset: header.phoff as usize,
        program_header_entry_size: header.phentsize as usize,
        program_header_count: header.phnum as usize,
    })
}

fn parse_header(bytes: &[u8]) -> Result<ElfHeader, ElfLoaderError> {
    if bytes.len() < 16 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    if &bytes[0..4] != ELF_MAGIC {
        return Err(ElfLoaderError::InvalidMagic);
    }

    let class = match bytes[EI_CLASS] {
        ELFCLASS32 => ElfClass::Elf32,
        ELFCLASS64 => ElfClass::Elf64,
        _ => return Err(ElfLoaderError::UnsupportedClass),
    };

    if bytes[EI_VERSION] != 1 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    let endian = match bytes[EI_DATA] {
        ELFDATA2LSB => Endianness::Little,
        _ => return Err(ElfLoaderError::UnsupportedEndianness),
    };

    match class {
        ElfClass::Elf64 => parse_header64(bytes, endian),
        ElfClass::Elf32 => parse_header32(bytes, endian),
    }
}

fn parse_header64(bytes: &[u8], endian: Endianness) -> Result<ElfHeader, ElfLoaderError> {
    if bytes.len() < 64 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    Ok(ElfHeader {
        class: ElfClass::Elf64,
        endian,
        machine: read_u16(bytes, 18, endian)?,
        entry: read_u64(bytes, 24, endian)?,
        phoff: read_u64(bytes, 32, endian)?,
        shoff: read_u64(bytes, 40, endian)?,
        phentsize: read_u16(bytes, 54, endian)?,
        phnum: read_u16(bytes, 56, endian)?,
        shentsize: read_u16(bytes, 58, endian)?,
        shnum: read_u16(bytes, 60, endian)?,
    })
}

fn parse_header32(bytes: &[u8], endian: Endianness) -> Result<ElfHeader, ElfLoaderError> {
    if bytes.len() < 52 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    Ok(ElfHeader {
        class: ElfClass::Elf32,
        endian,
        machine: read_u16(bytes, 18, endian)?,
        entry: read_u32(bytes, 24, endian)? as u64,
        phoff: read_u32(bytes, 28, endian)? as u64,
        shoff: read_u32(bytes, 32, endian)? as u64,
        phentsize: read_u16(bytes, 42, endian)?,
        phnum: read_u16(bytes, 44, endian)?,
        shentsize: read_u16(bytes, 46, endian)?,
        shnum: read_u16(bytes, 48, endian)?,
    })
}

fn validate_machine(header: &ElfHeader, arch: TargetArch) -> Result<(), ElfLoaderError> {
    let expected = match arch {
        TargetArch::X86_64 => 62,
        TargetArch::AArch64 => 183,
    };

    if header.machine != expected {
        return Err(ElfLoaderError::UnsupportedMachine);
    }
    Ok(())
}

fn parse_program_headers(
    bytes: &[u8],
    header: &ElfHeader,
) -> Result<Vec<ProgramHeader>, ElfLoaderError> {
    let phoff = header.phoff as usize;
    let entsize = header.phentsize as usize;
    if entsize == 0 {
        return Err(ElfLoaderError::InvalidProgramHeader);
    }

    let mut headers = Vec::new();
    for idx in 0..header.phnum as usize {
        let start = phoff + idx * entsize;
        let end = start + entsize;
        if end > bytes.len() {
            return Err(ElfLoaderError::UnexpectedEof);
        }
        let slice = &bytes[start..end];
        headers.push(parse_program_header(slice, header.class, header.endian)?);
    }
    Ok(headers)
}

fn parse_program_header(
    bytes: &[u8],
    class: ElfClass,
    endian: Endianness,
) -> Result<ProgramHeader, ElfLoaderError> {
    match class {
        ElfClass::Elf64 => parse_program_header64(bytes, endian),
        ElfClass::Elf32 => parse_program_header32(bytes, endian),
    }
}

fn parse_program_header64(bytes: &[u8], endian: Endianness) -> Result<ProgramHeader, ElfLoaderError> {
    if bytes.len() < 56 {
        return Err(ElfLoaderError::InvalidProgramHeader);
    }

    Ok(ProgramHeader {
        typ: read_u32(bytes, 0, endian)?,
        flags: read_u32(bytes, 4, endian)?,
        offset: read_u64(bytes, 8, endian)?,
        vaddr: read_u64(bytes, 16, endian)?,
        filesz: read_u64(bytes, 32, endian)?,
        memsz: read_u64(bytes, 40, endian)?,
        align: read_u64(bytes, 48, endian)?,
    })
}

fn parse_program_header32(bytes: &[u8], endian: Endianness) -> Result<ProgramHeader, ElfLoaderError> {
    if bytes.len() < 32 {
        return Err(ElfLoaderError::InvalidProgramHeader);
    }

    Ok(ProgramHeader {
        typ: read_u32(bytes, 0, endian)?,
        offset: read_u32(bytes, 4, endian)? as u64,
        vaddr: read_u32(bytes, 8, endian)? as u64,
        filesz: read_u32(bytes, 16, endian)? as u64,
        memsz: read_u32(bytes, 20, endian)? as u64,
        flags: read_u32(bytes, 24, endian)?,
        align: read_u32(bytes, 28, endian)? as u64,
    })
}

fn parse_section_headers(
    bytes: &[u8],
    header: &ElfHeader,
) -> Result<Vec<SectionHeader>, ElfLoaderError> {
    if header.shoff == 0 || header.shentsize == 0 {
        return Ok(Vec::new());
    }

    let mut sections = Vec::new();
    let shoff = header.shoff as usize;
    let entsize = header.shentsize as usize;
    for idx in 0..header.shnum as usize {
        let start = shoff + idx * entsize;
        let end = start + entsize;
        if end > bytes.len() {
            return Err(ElfLoaderError::UnexpectedEof);
        }
        let slice = &bytes[start..end];
        sections.push(parse_section_header(slice, header.class, header.endian)?);
    }
    Ok(sections)
}

fn parse_section_header(
    bytes: &[u8],
    class: ElfClass,
    endian: Endianness,
) -> Result<SectionHeader, ElfLoaderError> {
    match class {
        ElfClass::Elf64 => parse_section_header64(bytes, endian),
        ElfClass::Elf32 => parse_section_header32(bytes, endian),
    }
}

fn parse_section_header64(bytes: &[u8], endian: Endianness) -> Result<SectionHeader, ElfLoaderError> {
    if bytes.len() < 64 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    Ok(SectionHeader {
        sh_type: read_u32(bytes, 4, endian)?,
        sh_offset: read_u64(bytes, 24, endian)?,
        sh_size: read_u64(bytes, 32, endian)?,
        sh_entsize: read_u64(bytes, 56, endian)?,
    })
}

fn parse_section_header32(bytes: &[u8], endian: Endianness) -> Result<SectionHeader, ElfLoaderError> {
    if bytes.len() < 40 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    Ok(SectionHeader {
        sh_type: read_u32(bytes, 4, endian)?,
        sh_offset: read_u32(bytes, 16, endian)? as u64,
        sh_size: read_u32(bytes, 20, endian)? as u64,
        sh_entsize: read_u32(bytes, 36, endian)? as u64,
    })
}

fn parse_relocations(
    bytes: &[u8],
    header: &ElfHeader,
    sections: &[SectionHeader],
    arch: TargetArch,
) -> Result<Vec<Relocation>, ElfLoaderError> {
    let mut relocs = Vec::new();
    for section in sections {
        if section.sh_type != SHT_RELA && section.sh_type != SHT_REL {
            continue;
        }

        let entsize = if section.sh_entsize == 0 {
            match header.class {
                ElfClass::Elf64 => 24,
                ElfClass::Elf32 => 12,
            }
        } else {
            section.sh_entsize as usize
        };

        let mut offset = section.sh_offset as usize;
        let end = offset + section.sh_size as usize;
        if end > bytes.len() {
            return Err(ElfLoaderError::UnexpectedEof);
        }

        while offset < end {
            relocs.push(parse_relocation(
                &bytes[offset..offset + entsize],
                header.class,
                header.endian,
                section.sh_type,
                arch,
            )?);
            offset += entsize;
        }
    }
    Ok(relocs)
}

fn parse_relocation(
    bytes: &[u8],
    class: ElfClass,
    endian: Endianness,
    sh_type: u32,
    arch: TargetArch,
) -> Result<Relocation, ElfLoaderError> {
    match class {
        ElfClass::Elf64 => parse_relocation64(bytes, endian, sh_type, arch),
        ElfClass::Elf32 => parse_relocation32(bytes, endian, sh_type, arch),
    }
}

fn parse_relocation64(
    bytes: &[u8],
    endian: Endianness,
    sh_type: u32,
    arch: TargetArch,
) -> Result<Relocation, ElfLoaderError> {
    if bytes.len() < 24 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    let offset = read_u64(bytes, 0, endian)?;
    let info = read_u64(bytes, 8, endian)?;
    let addend = if sh_type == SHT_RELA {
        read_i64(bytes, 16, endian)?
    } else {
        0
    };

    let kind = match arch {
        TargetArch::X86_64 => {
            if (info as u32) != R_X86_64_RELATIVE {
                return Err(ElfLoaderError::RelocationUnsupported);
            }
            RelocationKind::X86Relative
        }
        TargetArch::AArch64 => {
            if (info as u32) != R_AARCH64_RELATIVE {
                return Err(ElfLoaderError::RelocationUnsupported);
            }
            RelocationKind::AArch64Relative
        }
    };

    Ok(Relocation { offset, addend, kind })
}

fn parse_relocation32(
    bytes: &[u8],
    endian: Endianness,
    sh_type: u32,
    arch: TargetArch,
) -> Result<Relocation, ElfLoaderError> {
    if bytes.len() < 12 {
        return Err(ElfLoaderError::InvalidHeader);
    }

    let offset = read_u32(bytes, 0, endian)? as u64;
    let info = read_u32(bytes, 4, endian)?;
    let addend = if sh_type == SHT_RELA {
        read_i32(bytes, 8, endian)? as i64
    } else {
        0
    };

    let kind = match arch {
        TargetArch::X86_64 => {
            if (info & 0xff) != R_X86_64_RELATIVE {
                return Err(ElfLoaderError::RelocationUnsupported);
            }
            RelocationKind::X86Relative
        }
        TargetArch::AArch64 => {
            if (info & 0xff) != R_AARCH64_RELATIVE {
                return Err(ElfLoaderError::RelocationUnsupported);
            }
            RelocationKind::AArch64Relative
        }
    };

    Ok(Relocation { offset, addend, kind })
}

fn build_segments(
    bytes: &[u8],
    header: &ElfHeader,
    program_headers: &[ProgramHeader],
) -> Result<Vec<Segment>, ElfLoaderError> {
    let mut segments = Vec::new();

    for ph in program_headers {
        if ph.typ != PT_LOAD || ph.memsz == 0 {
            continue;
        }

        let file_start = ph.offset as usize;
        let file_end = file_start + ph.filesz as usize;
        if file_end > bytes.len() {
            return Err(ElfLoaderError::UnexpectedEof);
        }

        let mut data = vec![0; ph.memsz as usize];
        let copy_len = core::cmp::min(ph.filesz as usize, data.len());
        data[..copy_len].copy_from_slice(&bytes[file_start..file_start + copy_len]);

        let mut flags = PageFlags::USER_ACCESSIBLE | PageFlags::PRESENT;
        if ph.flags & PF_WRITE != 0 {
            flags |= PageFlags::WRITABLE;
        }
        if ph.flags & PF_EXECUTE == 0 {
            flags |= PageFlags::NO_EXECUTE;
        }

        segments.push(Segment {
            vaddr: VirtAddr::new(ph.vaddr),
            mem_size: ph.memsz as usize,
            file_size: ph.filesz as usize,
            flags,
            data,
        });
    }

    segments.sort_by(|a, b| a.vaddr.0.cmp(&b.vaddr.0));
    Ok(segments)
}

fn apply_relocations(
    segments: &mut [Segment],
    relocations: &[Relocation],
    arch: TargetArch,
    class: ElfClass,
) -> Result<(), ElfLoaderError> {
    if relocations.is_empty() {
        return Ok(());
    }

    for reloc in relocations {
        if let Some(segment) = find_segment_mut(segments, reloc.offset) {
            let offset = (reloc.offset - segment.vaddr.0) as usize;
            match (arch, reloc.kind) {
                (TargetArch::X86_64, RelocationKind::X86Relative)
                | (TargetArch::AArch64, RelocationKind::AArch64Relative) => {
                    let bytes = match class {
                        ElfClass::Elf32 => 4,
                        ElfClass::Elf64 => 8,
                    };
                    if offset + bytes > segment.data.len() {
                        return Err(ElfLoaderError::UnexpectedEof);
                    }
                    let value = reloc.addend as u64;
                    if bytes == 4 {
                        segment.data[offset..offset + 4]
                            .copy_from_slice(&(value as u32).to_le_bytes());
                    } else {
                        segment.data[offset..offset + 8]
                            .copy_from_slice(&value.to_le_bytes());
                    }
                }
                _ => return Err(ElfLoaderError::RelocationUnsupported),
            }
        }
    }
    Ok(())
}

fn find_segment_mut<'a>(segments: &'a mut [Segment], addr: u64) -> Option<&'a mut Segment> {
    segments.iter_mut().find(|segment| {
        addr >= segment.vaddr.0 && addr < segment.vaddr.0 + segment.mem_size as u64
    })
}

fn build_auxiliary(header: &ElfHeader, argc: usize, envc: usize) -> Vec<AuxEntry> {
    vec![
        AuxEntry { key: 3, value: header.phoff as usize },   // AT_PHDR
        AuxEntry { key: 4, value: header.phentsize as usize }, // AT_PHENT
        AuxEntry { key: 5, value: header.phnum as usize },    // AT_PHNUM
        AuxEntry { key: 6, value: PAGE_SIZE },                // AT_PAGESZ
        AuxEntry { key: 7, value: 0 },                        // AT_BASE (not used)
        AuxEntry { key: 9, value: header.entry as usize },    // AT_ENTRY
        AuxEntry { key: 11, value: 0 },                       // AT_UID
        AuxEntry { key: 12, value: 0 },                       // AT_EUID
        AuxEntry { key: 13, value: 0 },                       // AT_GID
        AuxEntry { key: 14, value: 0 },                       // AT_EGID
        AuxEntry { key: 17, value: argc },                    // AT_EXECFN surrogate for argc
        AuxEntry { key: 23, value: envc },                    // AT_SECURE surrogate for env count
        AuxEntry { key: 0, value: 0 },                        // AT_NULL
    ]
}

fn build_stack(
    address_space: &AddressSpace,
    argv: &[&str],
    envp: &[&str],
    aux: &[AuxEntry],
) -> Result<StackImage, ElfLoaderError> {
    fn write_u64(buf: &mut [u8], offset: usize, value: u64) -> Result<(), ElfLoaderError> {
        let slice = buf
            .get_mut(offset..offset + 8)
            .ok_or(ElfLoaderError::StackOverflow)?;
        slice.copy_from_slice(&value.to_le_bytes());
        Ok(())
    }

    let argc = argv.len();
    let envc = envp.len();
    let ptr_size = core::mem::size_of::<u64>();

    let header_words = 1 + (argc + 1) + (envc + 1) + aux.len() * 2;
    let header_bytes = header_words * ptr_size;

    let strings_size: usize = argv
        .iter()
        .chain(envp.iter())
        .map(|entry| entry.as_bytes().len() + 1)
        .sum();

    let strings_offset = align_up(header_bytes, 16);
    let total = align_up(strings_offset + strings_size, 16);

    let top = address_space.stack_end.0 as usize;
    let bottom_limit = address_space.stack_start.0 as usize;

    if total > top.saturating_sub(bottom_limit) {
        return Err(ElfLoaderError::StackOverflow);
    }

    let user_sp = (top - total) & !0xF;
    let user_sp_addr = user_sp as u64;

    let argv_offset = ptr_size;
    let envp_offset = argv_offset + (argc + 1) * ptr_size;
    let aux_offset = envp_offset + (envc + 1) * ptr_size;

    let mut data = vec![0u8; total];

    write_u64(&mut data, 0, argc as u64)?;

    let mut str_cursor = strings_offset;
    for (idx, arg) in argv.iter().enumerate() {
        let bytes = arg.as_bytes();
        let needed = bytes.len() + 1;
        if str_cursor + needed > data.len() {
            return Err(ElfLoaderError::StackOverflow);
        }

        data[str_cursor..str_cursor + bytes.len()].copy_from_slice(bytes);
        data[str_cursor + bytes.len()] = 0;

        let ptr = user_sp_addr + str_cursor as u64;
        write_u64(&mut data, argv_offset + idx * ptr_size, ptr)?;

        str_cursor += needed;
    }

    write_u64(&mut data, argv_offset + argc * ptr_size, 0)?;

    for (idx, env) in envp.iter().enumerate() {
        let bytes = env.as_bytes();
        let needed = bytes.len() + 1;
        if str_cursor + needed > data.len() {
            return Err(ElfLoaderError::StackOverflow);
        }

        data[str_cursor..str_cursor + bytes.len()].copy_from_slice(bytes);
        data[str_cursor + bytes.len()] = 0;

        let ptr = user_sp_addr + str_cursor as u64;
        write_u64(&mut data, envp_offset + idx * ptr_size, ptr)?;

        str_cursor += needed;
    }

    write_u64(&mut data, envp_offset + envc * ptr_size, 0)?;

    for (idx, entry) in aux.iter().enumerate() {
        write_u64(&mut data, aux_offset + idx * 2 * ptr_size, entry.key as u64)?;
        write_u64(
            &mut data,
            aux_offset + (idx * 2 + 1) * ptr_size,
            entry.value as u64,
        )?;
    }

    Ok(StackImage {
        user_sp: VirtAddr::new(user_sp_addr),
        stack_top: address_space.stack_end,
        stack_bottom: address_space.stack_start,
        data,
    })
}

fn align_up(value: usize, align: usize) -> usize {
    if align == 0 {
        return value;
    }
    (value + align - 1) & !(align - 1)
}

fn read_u16(bytes: &[u8], offset: usize, endian: Endianness) -> Result<u16, ElfLoaderError> {
    let slice = bytes.get(offset..offset + 2).ok_or(ElfLoaderError::UnexpectedEof)?;
    Ok(match endian {
        Endianness::Little => u16::from_le_bytes(slice.try_into().unwrap()),
        Endianness::Big => u16::from_be_bytes(slice.try_into().unwrap()),
    })
}

fn read_u32(bytes: &[u8], offset: usize, endian: Endianness) -> Result<u32, ElfLoaderError> {
    let slice = bytes.get(offset..offset + 4).ok_or(ElfLoaderError::UnexpectedEof)?;
    Ok(match endian {
        Endianness::Little => u32::from_le_bytes(slice.try_into().unwrap()),
        Endianness::Big => u32::from_be_bytes(slice.try_into().unwrap()),
    })
}

fn read_i32(bytes: &[u8], offset: usize, endian: Endianness) -> Result<i32, ElfLoaderError> {
    read_u32(bytes, offset, endian).map(|value| value as i32)
}

fn read_u64(bytes: &[u8], offset: usize, endian: Endianness) -> Result<u64, ElfLoaderError> {
    let slice = bytes.get(offset..offset + 8).ok_or(ElfLoaderError::UnexpectedEof)?;
    Ok(match endian {
        Endianness::Little => u64::from_le_bytes(slice.try_into().unwrap()),
        Endianness::Big => u64::from_be_bytes(slice.try_into().unwrap()),
    })
}

fn read_i64(bytes: &[u8], offset: usize, endian: Endianness) -> Result<i64, ElfLoaderError> {
    read_u64(bytes, offset, endian).map(|value| value as i64)
}

#[cfg(test)]
mod tests {
    use super::*;

    const MINIMAL_ELF: &[u8] = include_bytes!("../../tests/data/minimal-x86_64.elf");

    fn dummy_space() -> AddressSpace {
        AddressSpace {
            page_table_frame: crate::memory::frame_allocator::Frame {
                start_address: crate::memory::PhysAddr::new(0),
            },
            heap_start: VirtAddr::new(0x1000_0000),
            heap_end: VirtAddr::new(0x2000_0000),
            stack_start: VirtAddr::new(0x0000_7FFF_FFFF_0000),
            stack_end: VirtAddr::new(0x0000_8000_0000_0000),
        }
    }

    #[test]
    fn parse_minimal_elf() {
        let image = load_executable(MINIMAL_ELF, TargetArch::X86_64, &dummy_space(), &[], &[])
            .expect("failed to load ELF");
        assert_eq!(image.segments.len(), 1);
        assert!(image.entry_point.0 > 0);
    }
}
