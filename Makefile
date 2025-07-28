.POSIX:
.PHONY: all clean bootloader gpt-tool image

# Default target
all: image

# Build the bootloader
bootloader:
	@echo "Building bootloader..."
	cd bootloader && cargo build --target x86_64-unknown-uefi --release
	@echo "Copying BOOTx64.efi to gpt-tool directory..."
	cp bootloader/target/x86_64-unknown-uefi/release/BOOTx64.efi gpt-tool/

# Build the gpt-tool
gpt-tool:
	@echo "Building gpt-tool..."
	cd gpt-tool && $(MAKE)

# Create the disk image (depends on both bootloader and gpt-tool)
image: bootloader gpt-tool
	@echo "Creating disk image..."
	cd gpt-tool && ./main
	@echo "Build complete! Disk image created at gpt-tool/test.img"

# Clean all build artifacts
clean:
	@echo "Cleaning bootloader..."
	cd bootloader && cargo clean
	@echo "Cleaning gpt-tool..."
	cd gpt-tool && $(MAKE) clean
	@echo "Removing copied BOOTx64.efi..."
	rm -f gpt-tool/BOOTx64.efi
	@echo "Clean complete!"

# Quick test with QEMU (if qemu.sh exists)
test: image
	@if [ -f gpt-tool/qemu.sh ]; then \
		echo "Running QEMU test..."; \
		cd gpt-tool && sh ./qemu.sh; \
	else \
		echo "qemu.sh not found, skipping test"; \
	fi