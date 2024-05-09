kafl fuzz --purge --bios ./targets/uboot/u-boot/u-boot.rom\
	-p 1\
	--image ./images/uboot-img \
	--seed-dir $1 \
	--payload-size 65536 \
	--grimoire -D --radamsa --redqueen --redqueen-hammer \
	--memory 512 \
	--t-hard 1 \
	--t-soft 0.1 \
	--trace \
