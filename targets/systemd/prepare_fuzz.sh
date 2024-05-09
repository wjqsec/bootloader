#! /bin/bash

cp ./harness/nyx_api.h ./systemd/src/boot/efi/
cp ./harness/paging_uefi_x64.h ./systemd/src/boot/efi/
cp ./harness/boot.c ./systemd/src/boot/efi/
cd ./systemd/builddir/
ninja
sudo losetup --partscan  /dev/loop40  ../../../../images/systemd.img
sudo mount /dev/loop40p1 ../../../../images/mount/
sudo cp ./src/boot/efi/systemd-bootx64.efi ../../../../images/mount/EFI/Boot/bootx64.efi
sudo umount ../../../../images/mount/
sudo losetup -d /dev/loop40

# meson setup builddir