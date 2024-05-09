#! /bin/bash
cp ./harness/main.c ./u-boot/common
cp ./harness/board_r.c ./u-boot/common
cp ./harness/paging.h ./u-boot/include
cp ./harness/nyx_api.h ./u-boot/include
cp ./harness/ide.c ./u-boot/drivers/block/
cp ./harness/net.c ./u-boot/cmd/
cp ./harness/nfs.c ./u-boot/net/
cp ./harness/e1000.c ./u-boot/drivers/net/
cp ./harness/time.c ./u-boot/lib/
cp ./harness/hang.c ./u-boot/lib/
cp ./harness/panic.c ./u-boot/lib/
cd ./u-boot
make HOSTCC=clang CC=clang -j16

# make qemu-x86_defconfig
# CONFIG_SYS_MALLOC_LEN=0xa00000
# CONFIG_CMD_SNTP=y
# CONFIG_CMD_DNS=y
# CONFIG_CMD_NFS=y
# CONFIG_CMD_WGET=y
# CONFIG_CMD_TFTPPUT=y
# CONFIG_CMD_TFTPSRV=y
# CONFIG_FS_BTRFS=y
# CONFIG_FS_CBFS=y
# CONFIG_FS_CRAMFS=y
# CONFIG_FS_SQUASHFS=y
# CONFIG_FS_EROFS=y
# CONFIG_CMD_REISER=y
# CONFIG_CMD_ZFS=y