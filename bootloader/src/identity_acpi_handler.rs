use acpi::{Handler, PhysicalMapping};
use acpi::aml::AmlError;

#[derive(Clone)]
pub struct IdentityAcpiHandler;

impl Handler for IdentityAcpiHandler {
    unsafe fn map_physical_region<T>(
        &self,
        physical_address: usize,
        size: usize,
    ) -> PhysicalMapping<Self, T> {
        // Ring 0 allows us to directly return
        PhysicalMapping {
            physical_start: physical_address,
            virtual_start: unsafe { core::ptr::NonNull::<T>::new_unchecked(physical_address as *mut T) },
            region_length: size,
            mapped_length: size,
            handler: Self,
        }
    }

    // No op because the region is always availabe in UEFI 
    fn unmap_physical_region<T>(_region: &PhysicalMapping<Self, T>) { }

    // TODO: Properly implement these in the future
    fn read_u8(&self, _address: usize) -> u8 { 0 }
    fn read_u16(&self, _address: usize) -> u16 { 0 }
    fn read_u32(&self, _address: usize) -> u32 { 0 }
    fn read_u64(&self, _address: usize) -> u64 { 0 }

    fn write_u8(&self, _address: usize, _value: u8) {}
    fn write_u16(&self, _address: usize, _value: u16) {}
    fn write_u32(&self, _address: usize, _value: u32) {}
    fn write_u64(&self, _address: usize, _value: u64) {}

    fn read_io_u8(&self, _port: u16) -> u8 { 0 }
    fn read_io_u16(&self, _port: u16) -> u16 { 0 }
    fn read_io_u32(&self, _port: u16) -> u32 { 0 }

    fn write_io_u8(&self, _port: u16, _value: u8) {}
    fn write_io_u16(&self, _port: u16, _value: u16) {}
    fn write_io_u32(&self, _port: u16, _value: u32) {}

    fn read_pci_u8(&self, _address: acpi::PciAddress, _offset: u16) -> u8 { 0 }
    fn read_pci_u16(&self, _address: acpi::PciAddress, _offset: u16) -> u16 { 0 }
    fn read_pci_u32(&self, _address: acpi::PciAddress, _offset: u16) -> u32 { 0 }

    fn write_pci_u8(&self, _address: acpi::PciAddress, _offset: u16, _value: u8) {}
    fn write_pci_u16(&self, _address: acpi::PciAddress, _offset: u16, _value: u16) {}
    fn write_pci_u32(&self, _address: acpi::PciAddress, _offset: u16, _value: u32) {}

    fn nanos_since_boot(&self) -> u64 { 0 }
    fn stall(&self, _microseconds: u64) {}
    fn sleep(&self, _milliseconds: u64) {}

    fn create_mutex(&self) -> acpi::Handle {
        acpi::Handle(0)
    }

    fn acquire(
        &self,
        _mutex: acpi::Handle,
        _timeout: u16,
    ) -> Result<(), AmlError> {
        Err(AmlError::LibUnimplemented)
    }

    fn release(&self, _mutex: acpi::Handle) {}
}