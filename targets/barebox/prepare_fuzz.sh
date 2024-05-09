#! /bin/bash
cp ./harness/startup.c ./barebox-2024.01.0/common/
cp ./harness/block.c ./barebox-2024.01.0/common/
cp ./harness/misc.c ./barebox-2024.01.0/common/
cp ./harness/image_renderer.c ./barebox-2024.01.0/lib/gui/
cp ./harness/efi-block-io.c ./barebox-2024.01.0/drivers/block/
cp ./harness/efi.c ./barebox-2024.01.0/fs/
cp ./harness/nfs.c ./barebox-2024.01.0/net/
cp ./harness/ping.c ./barebox-2024.01.0/net/
cp ./harness/sntp.c ./barebox-2024.01.0/net/
cp ./harness/efi-snp.c ./barebox-2024.01.0/drivers/net/
cp ./harness/nyx_api.h ./barebox-2024.01.0/include/
cp ./harness/paging_uefi_x64.h ./barebox-2024.01.0/include/
cd ./barebox-2024.01.0/
make 
sudo losetup --partscan  /dev/loop40  ../../../images/barebox.img
sudo mount /dev/loop40p1 ../../../images/mount/
sudo cp ./barebox.efi ../../../images/mount/EFI/Boot/bootx64.efi
sudo umount ../../../images/mount/
sudo losetup -d /dev/loop40

# export ARCH=x86
# make efi_defconfig
# CONFIG_NET_SNTP=y
# CONFIG_FS_CRAMFS=y
# CONFIG_FS_BPKFS=y
# CONFIG_FS_UIMAGEFS=y
# CONFIG_FS_PSTORE=y
# CONFIG_FS_SQUASHFS=y
# CONFIG_BASE64=y
# CONFIG_BMP=y
# CONFIG_PNG=y
# CONFIG_QOIy=y
# CONFIG_PICOPNG=y or # CONFIG_LODEPNG=y
# CONFIG_CMD_LOADS=y
