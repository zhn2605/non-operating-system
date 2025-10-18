#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::cell::RefCell;
use uefi::boot::{self, SearchType, ScopedProtocol};
use uefi::system; 
use uefi::table;
use uefi::prelude::*;
use uefi::proto::device_path::text::{
    AllowShortcuts, DevicePathToText, DisplayOnly,
};
use uefi::proto::loaded_image::LoadedImage;
use uefi::{Identify, Result};
use log::{info, warn, error};

mod cfg_table_type;
mod kernel_args;
use crate::cfg_table_type::CfgTableType;
use crate::kernel_args::KernelArgs;

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
    if list_kargs {
        info!("Empty karg: {:?}", karg.borrow());
        system::with_config_table(|config_table| {
            karg.borrow_mut().populate_from_cfg_table(config_table);
        });
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

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
