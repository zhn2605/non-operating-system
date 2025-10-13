#![no_std]
#![no_main]

use core::panic::PanicInfo;
use uefi::boot::{self, SearchType, ScopedProtocol};
use uefi::{system, table};
use uefi::prelude::*;
use uefi::proto::device_path::text::{
    AllowShortcuts, DevicePathToText, DisplayOnly,
};
use uefi::proto::loaded_image::LoadedImage;
use uefi::{Identify, Result};
use log::{info, warn, error};

#[entry]
fn main() -> Status {
    uefi::helpers::init().unwrap();
    info!("Hello world!");
    warn!("WARN test");
    error!("ERORR test");

    print_image_path().unwrap();

    wait_for_keypress();

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

fn wait_for_keypress() -> Result {
    info!("Press a key to continue...");

    system::with_stdin(|stdin| {
        stdin.reset(true)?;
        // Reset input buffer

        let mut key_press_event = unsafe { [stdin.wait_for_key_event().expect("Expected wait event")] };
        // Retrieve key press event

        boot::wait_for_event(&mut key_press_event).unwrap();
        // Note newer versions of UEFI automatically sets up systemtable and image handle

        Ok(())
    })
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
