use core::format_args;
use uefi::Guid;

#[derive(Debug)]
pub struct CfgTableType(uefi::Guid);

// Constructor that allows us to use .into() on any uefi::Guid to convert to CfgTableType
impl From<Guid> for CfgTableType {
    fn from(guid: Guid) -> Self {
        Self(guid)
    }
}

/*
 * This implements a Display trait that will output the human-readable name for any recognized GUIDs
 * while falling back to the Display implementation in uefi::Guid, for unrecognized ones.
 */
impl core::fmt::Display for CfgTableType {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::result::Result<(), core::fmt::Error> {
        match self.0 {
            uefi::table::cfg::ACPI2_GUID => f.write_str("ACPI2"),
            uefi::table::cfg::ACPI_GUID => f.write_str("ACPI1"),
            uefi::table::cfg::DEBUG_IMAGE_INFO_GUID => f.write_str("Debug Image"),
            uefi::table::cfg::DXE_SERVICES_GUID => f.write_str("DXE Services"),
            uefi::table::cfg::ESRT_GUID => f.write_str("EFI System Resources"),
            uefi::table::cfg::HAND_OFF_BLOCK_LIST_GUID => f.write_str("Hand-off Block List"),
            uefi::table::cfg::LZMA_COMPRESS_GUID => f.write_str("LZMA Compressed filesystem"),
            uefi::table::cfg::MEMORY_STATUS_CODE_RECORD_GUID => f.write_str("Hand-off Status Code"),
            uefi::table::cfg::MEMORY_TYPE_INFORMATION_GUID => f.write_str("Memory Type Information"),
            uefi::table::cfg::PROPERTIES_TABLE_GUID => f.write_str("Properties Table"),
            uefi::table::cfg::SMBIOS3_GUID => f.write_str("SMBIOS3"),
            uefi::table::cfg::SMBIOS_GUID => f.write_str("SMBIOS1"),
            uefi::table::cfg::TIANO_COMPRESS_GUID => f.write_str("Tiano compressed filesystem"),
            x => f.write_fmt(format_args!("{}", x)),
        }
    }
}