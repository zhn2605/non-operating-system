use core::ffi::c_void;
use uefi::table::cfg::{ACPI_GUID, ACPI2_GUID, ConfigTableEntry, SMBIOS_GUID, SMBIOS3_GUID};

#[derive(Copy, Clone, Debug)]
pub struct KernelArgs {
    acpi_ptr: *const c_void,
    smbios_ptr: *const c_void,
    acpi_ver: u8,
    smbios_ver: u8,
    // pointer to PCI Express Ecam space
    pcie_ptr: *mut c_void,
}

impl Default for KernelArgs {
    fn default() -> Self {
        Self {
            acpi_ptr: 0 as *const c_void,
            smbios_ptr: 0 as *const c_void,
            acpi_ver: 0,
            smbios_ver: 0,
            pcie_ptr: 0 as *mut c_void,
        }
    }
}

impl KernelArgs {
    pub fn populate_from_cfg_table(&mut self, cfg_tables: &[ConfigTableEntry]) {
        for cfg in cfg_tables {
            match cfg.guid {
                ACPI2_GUID => {
                    if self.acpi_ver < 2 {
                        self.acpi_ver = 2;
                        self.acpi_ptr = cfg.address;
                    }
                },
                ACPI_GUID => {
                    if self.acpi_ver < 1 {
                        self.acpi_ver = 1;
                        self.acpi_ptr = cfg.address;
                    }
                },
                SMBIOS3_GUID => {
                    if self.smbios_ver < 3 {
                        self.smbios_ver = 3;
                        self.smbios_ptr = cfg.address;
                    }
                }
                SMBIOS_GUID => {
                    if self.smbios_ver < 1 {
                        self.smbios_ver = 1;
                        self.smbios_ptr = cfg.address;
                    }
                }
                _ => {},
            }
        }
    }

    pub fn get_acpi(&self) -> (*const c_void, u8) {
        (self.acpi_ptr, self.acpi_ver)
    }

    pub fn get_smbios(&self) -> (*const c_void, u8) {
        (self.smbios_ptr, self.smbios_ver)
    }

    pub fn set_pcie(&mut self, ptr: *mut c_void) {
        self.pcie_ptr = ptr
    }

    pub fn get_pcie(&self) -> *mut c_void {
        self.pcie_ptr
    }
}