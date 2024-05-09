#! /bin/bash
cp ./harness/entry.s2.c ./limine/common/
cp ./harness/nyx_api.h ./limine/common/  
cp ./harness/paging.h ./limine/common/
cp ./harness/panic.s2.c ./limine/common/lib/
cp ./harness/real.h ./limine/common/lib/ 
cp ./harness/real.s2.asm_bios_ia32 ./limine/common/lib/  
cd ./limine/ 
make  
./bin/limine bios-install ../../../images/limine_mbr
# sudo losetup --partscan  /dev/loop40  ../../../images/limine_mbr
# sudo mount /dev/loop40p1 ../../../images/mount/
# sudo cp ./bin/limine-bios.sys ../../../images/mount/
# sudo umount ../../../images/mount/
# sudo losetup -d  /dev/loop40

# ./configure --enable-bios