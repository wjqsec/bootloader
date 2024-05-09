#! /bin/bash

cp ./harness/main.c ./refind-code/refind/
cp ./harness/fsw_efi.c ./refind-code/filesystems/
cp ./harness/nyx_api.h ./refind-code/include/
cp ./harness/driver_support.c ./refind-code/refind/
cp ./harness/paging_uefi_x64.h ./refind-code/include/
cd ./refind-code
make all_gnuefi
sudo losetup --partscan  /dev/loop40  ./../../../images/refind.img
sudo mount /dev/loop40p1 ./../../../images/mount/
sudo cp ./refind/refind_x64.efi ./../../../images/mount/EFI/Boot/bootx64.efi
sudo cp ./drivers_x64/*.efi ./../../../images/mount/EFI/Boot/drivers/
sudo cp ./gptsync/gptsync_x64.efi ./../../../images/mount/EFI/refind/
sudo umount ./../../../images/mount/
sudo losetup -d /dev/loop40