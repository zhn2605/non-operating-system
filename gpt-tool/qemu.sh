#!/bin/sh

qemu-system-x86_64 \
-drive format=raw,file=test.img \
-bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
-name macaroni-OS \
-net none