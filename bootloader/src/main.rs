#![no_std]
#![no_main]

mod cfg_table_type;
mod kernel_args;
mod identity_acpi_handler;
mod os_mem;
mod gop;

use core::cell::RefCell;
use acpi::{platform::PciConfigRegions, AcpiTables};
use uefi::{
    boot::{self, SearchType}, mem::memory_map::{self, MemoryMap, MemoryMapMeta}, prelude::*, proto::{console::gop::GraphicsOutput, device_path::text::{
        AllowShortcuts, DevicePathToText, DisplayOnly,
    }, loaded_image::LoadedImage}, system, table, Identify, Result, Status
};
use log::{info, warn, error};

use crate::cfg_table_type::CfgTableType;
use crate::kernel_args::KernelArgs;
use crate::identity_acpi_handler::IdentityAcpiHandler;
use crate::os_mem::OSMemEntry;
use crate::gop::Gop;

#[entry]
fn main() -> Status {
    uefi::helpers::init().unwrap();
    // Note newer versions of UEFI automatically sets up systemtable and image handle

    info!("Hello world!");
    warn!("WARN test");
    error!("ERORR test");

    print_image_path().unwrap();
    wait_for_keypress().unwrap();

    let list_info = true;
    populate_karg(list_info).unwrap();
    wait_for_keypress().unwrap();

    // init frambuffer
    let mut gop = Gop::init().unwrap();
    warn!("Will switch over to graphic output after the following keypress!");
    wait_for_keypress().unwrap();

    gop.set_mode().unwrap();

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

fn populate_karg(list_info : bool) -> Result {
    if list_info {
        info!("Image Handle: {:#018x}", boot::image_handle as usize);
        info!("System Table: {:#018x}", table::system_table_raw as usize);
        info!("UEFI Revision: {}", system::uefi_revision());
    }

    info!("List of CFGs: ");
    system::with_config_table(|config_table| {
        for cfg in config_table {
            let cfg_table_name: CfgTableType = cfg.guid.into();
            if list_info { info!("Ptr: {:#018x}, GUID: {}", cfg.address as usize, cfg_table_name); }
        }
    });

    let karg = RefCell::new(KernelArgs::default());
    // Necessary for interior mutability because with_config_table forces non mutable Fn
    // info!("Empty karg: {:?}", karg.borrow());
    system::with_config_table(|config_table| {
        karg.borrow_mut().populate_from_cfg_table(config_table);
    });

    let ih: IdentityAcpiHandler  = IdentityAcpiHandler;
    let acpi_tables = unsafe { AcpiTables::from_rsdp(ih, karg.borrow().get_acpi().0 as usize)}.unwrap();
    
    let pcie_cfg = PciConfigRegions::new(&acpi_tables).unwrap();
    // loop through all poossible segments for pcie
    for sg in 0u16..=65535u16 {
        if let Some(addr) = pcie_cfg.physical_address(sg, 0, 0, 0) {
            // populate first segment into karg presuming it attaches to everything needed for basic computing functions (AHCI, GPU, USB)
            karg.borrow_mut().set_pcie(addr as *mut core::ffi::c_void);
            if list_info { info!("PCIe({}, 0, 0, 0): {:#018x}", sg, addr); }
            break;
        }
    }

    if list_info {
        info!("ACPI Revision: {}", acpi_tables.rsdp_revision);
        info!("Populated karg: {:?}", karg.borrow());
    }

    let (mm_ptr, count) = get_mm();
    karg.borrow_mut().set_memmap(mm_ptr, count);

    info!("Got memory");
    info!("karg after MemMap: {:?}", karg);

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

fn get_mm() -> (*mut OSMemEntry, usize) {
    let mm_owned = boot::memory_map(memory_map::MemoryType::BOOT_SERVICES_DATA).unwrap();
    // Retrieve ncessary map info
    let meta: MemoryMapMeta = mm_owned.meta();
    let entry_count: usize = meta.entry_count();

    //  Allocate runtime buffer to store translated osmm entry list
    let alloc_bytes: usize = entry_count
        .checked_mul(size_of::<OSMemEntry>()).unwrap();

    let runtime_ptr_nonnull = boot::allocate_pool(memory_map::MemoryType::RUNTIME_SERVICES_DATA, alloc_bytes).unwrap();

   // Convert NonNull<u8> -> *mut OSMemEntry
    let mementry_ptr: *mut OSMemEntry = runtime_ptr_nonnull.as_ptr() as *mut OSMemEntry;

    //  Construct a temporary safe slice to write into
    let mementries: &mut [OSMemEntry] = unsafe {
        // safe to create slice from alloc bytes
        core::slice::from_raw_parts_mut(mementry_ptr, entry_count)
    };

    // translate MemDiscriptor into an OSMemEntry
    let mut num_entries: usize = 0;
    for (i, desc) in mm_owned.entries().enumerate() {
        if i >= entry_count { break; }
        mementries[i] = desc.into();
        num_entries += 1;
    }

    // Return the raw pointer and the actual number of entries written
    (mementry_ptr, num_entries)
}

// #[panic_handler]
// fn panic(_info: &PanicInfo) -> ! {
//     error!("[PANIC]: {} at \n{:?}", _info.message(), _info.location());
//     loop {}
// }
