use uefi::boot;
use uefi::mem::memory_map;

#[derive(Copy, Clone, Debug)]
pub struct OSMemEntry {
    pub mem_type: boot::MemoryType,
    pub base: usize,
    pub pages: usize,
    pub mem_attrib: boot::MemoryAttribute,
}

impl From<&boot::MemoryDescriptor> for OSMemEntry {
    fn from(mdesc: &memory_map::MemoryDescriptor) -> OSMemEntry {
        OSMemEntry {
            mem_type: mdesc.ty,
            base: mdesc.phys_start as usize,
            pages: mdesc.page_count as usize,
            mem_attrib: mdesc.att,
        }
    }
}