#! /bin/bash
cp ./harness/loader_x86.c ./easyboot/src/
cp ./harness/nyx_api.h ./easyboot/src/
cp ./harness/paging_uefi_x64.h ./easyboot/src/
cd ./easyboot/src
rm ./loader_x86.efi
make clean
make loader_x86.efi
sudo losetup --partscan  /dev/loop40  ../../../../images/easyboot.img
sudo mount /dev/loop40p1 ../../../../images/mount/
sudo cp ./loader_x86.efi ../../../../images/mount/EFI/Boot/bootx64.efi
sudo cp ./plugins/exfat_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/axfs_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/befs_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/ext234_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/fat1x_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/fsZ_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/mfs3_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/ntfs_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/ufs2_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/ufs44_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ./plugins/xfs_x86_64.plg ../../../../images/mount/easyboot/
sudo cp ../docs/en/menu.cfg ../../../../images/mount/easyboot/
sudo ls ../../../../images/mount/easyboot/
sudo umount ../../../../images/mount/
sudo losetup -d /dev/loop40


# -fdirect-access-external-data to -mpie-copy-relocations