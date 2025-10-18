#![no_std]
#![no_main]

mod cfg_table_type;
mod kernel_args;
mod identity_acpi_handler;

use core::panic::PanicInfo;
use core::cell::RefCell;
use acpi::{platform::PciConfigRegions, AcpiTables};
use uefi::{
    boot::{self, SearchType},
    system,
    table,
    prelude::*,
    proto::device_path::text::{
        AllowShortcuts, DevicePathToText, DisplayOnly,
    },
    proto::loaded_image::LoadedImage,
    Identify, 
    Result,
};
use log::{info, warn, error};

use crate::cfg_table_type::CfgTableType;
use crate::kernel_args::KernelArgs;
use crate::identity_acpi_handler::IdentityAcpiHandler;

#[entry]
fn main() -> Status {
    uefi::helpers::init().unwrap();
    // Note newer versions of UEFI automatically sets up systemtable and image handle
    
    info!("Hello world!");
    warn!("WARN test");
    error!("ERORR test");

    print_image_path().unwrap();

    let list_cfg = false;
    let list_kargs = true;
    list_info(list_cfg, list_kargs).unwrap();

    wait_for_keypress().unwrap();

    Status::SUCCESS
}

fn print_image_path() -> Result {
    let handle = boot::open_protocol_exclusive::<LoadedImage>(boot::image_handle())
    .expect("Unable to open image handle");

    let device_path_to_text_handle = *boot::locate_handle_buffer(
        SearchType::ByProtocol(&DevicePathToText::GUID),
    )?
    .first()
    .expect("DevicePathToText is missing");

    let device_path_to_text = boot::open_protocol_exclusive::<DevicePathToText>(
        device_path_to_text_handle,
    )?;

    let image_device_path =
        handle.file_path().expect("File path is not set");
    let image_device_path_text = device_path_to_text
        .convert_device_path_to_text(
            image_device_path,
            DisplayOnly(true),
            AllowShortcuts(false),
        )
        .expect("convert_device_path_to_text failed");

    info!("Image path: {}", &*image_device_path_text);
    Ok(())
}

fn list_info(list_cfg : bool, list_kargs: bool) -> Result {
    info!("Image Handle: {:#018x}", boot::image_handle as usize);
    info!("System Table: {:#018x}", table::system_table_raw as usize);
    info!("UEFI Revision: {}", system::uefi_revision());

    if list_cfg {
        info!("List of CFGs: ");
        system::with_config_table(|config_table| {
            for cfg in config_table {
                let cfg_table_name: CfgTableType = cfg.guid.into();
                info!("Ptr: {:#018x}, GUID: {}", cfg.address as usize, cfg_table_name);
            }
        });
    }

    let karg = RefCell::new(KernelArgs::default());
    // Necessary for interior mutability because with_config_table forces non mutable Fn
    info!("Empty karg: {:?}", karg.borrow());
    system::with_config_table(|config_table| {
        karg.borrow_mut().populate_from_cfg_table(config_table);
    });

    let ih: IdentityAcpiHandler  = IdentityAcpiHandler;
    let acpi_tables = unsafe { AcpiTables::from_rsdp(ih, karg.borrow().get_acpi().0 as usize)}.unwrap();
    info!("ACPI Revision: {}", acpi_tables.rsdp_revision);

    let pcie_cfg = PciConfigRegions::new(&acpi_tables).unwrap();
    // loop through all poossible segments for pcie
    for sg in 0u16..=65535u16 {
        if let Some(addr) = pcie_cfg.physical_address(sg, 0, 0, 0) {
            // populate first segment into karg presuming it attaches to everything needed for basic computing functions (AHCI, GPU, USB)
            karg.borrow_mut().set_pcie(addr as *mut core::ffi::c_void);
            info!("PCIe({}, 0, 0, 0): {:#018x}", sg, addr);
            break;
        }
    }

    if list_kargs {
        info!("Populated karg: {:?}", karg.borrow());
    }

    Ok(())
}

fn wait_for_keypress() -> Result {
    info!("Press a key to continue...");

    system::with_stdin(|stdin| {
        stdin.reset(true)?;
        // Reset input buffer

        let mut key_press_event = [stdin.wait_for_key_event().expect("Expected wait event")];
        // Retrieve key press event

        boot::wait_for_event(&mut key_press_event).unwrap();

        Ok(())
    })
}

// #[panic_handler]
// fn panic(_info: &PanicInfo) -> ! {
//     error!("[PANIC]: {} at \n{:?}", _info.message(), _info.location());
//     loop {}
// }
