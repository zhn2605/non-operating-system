#!/bin/sh

qemu-system-x86_64 -enable-kvm-machine q35\
-device ide-hd,drive=disk0,model=NOS\ Boot\ Manager,serial=NOSDISK \
-drive id=disk0,format=raw,file=test.img,if=none \
-bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
-name NOS \
-net none
