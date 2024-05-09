#! /bin/bash

cp ./harness/nyx_api.h ./CloverBootloader/Include/
cp ./harness/paging_uefi_x64.h ./CloverBootloader/Include/
cp ./harness/main.cpp ./CloverBootloader/rEFIt_UEFI/refit/
cp ./harness/fsw_efi.c ./CloverBootloader/FileSystems/VBoxFsDxe/
cp ./harness/BdsConnect.cpp ./CloverBootloader/rEFIt_UEFI/Platform/
cp ./harness/ConfigManager.cpp ./CloverBootloader/rEFIt_UEFI/Settings/
cd ./CloverBootloader/
./ebuild.sh -fr
cp ./CloverPackage/CloverV2/EFI/CLOVER/drivers/off/UEFI/FileSystem/* ./CloverPackage/CloverV2/EFI/CLOVER/drivers/UEFI/
cp ./Build/Clover/RELEASE_GCC53/X64/AppleImageCodec.efi ./CloverPackage/CloverV2/EFI/CLOVER/drivers/UEFI/
sudo losetup --partscan  /dev/loop40  ./../../../images/clover.img
sudo mount /dev/loop40p1 ./../../../images/mount/
sudo cp -rf ./CloverPackage/CloverV2/EFI ./../../../images/mount/
sudo cp ./Build/Clover/RELEASE_GCC53/X64/AppleImageCodec.efi ./../../../images/mount/
sudo umount ./../../../images/mount/
sudo losetup -d /dev/loop40


# Clover.dsc 
# git clone --recurse https://github.com/CloverHackyColor/CloverBootloader.git
# cd CloverBootloader
# ./edksetup.sh
# ./ebuild.sh -fr
# Protocols/AppleImageCodec/AppleImageCodec.inf