kafl fuzz --purge --image ./images/easyboot.img --bios /usr/share/qemu/OVMF.fd  --qemu-extra ""\
	-p 1 \
	--seed-dir $1 \
	--payload-size 65536 \
	--grimoire -D --radamsa --redqueen --redqueen-hammer \
	--memory 512 \
	--t-hard 1 \
	--t-soft 0.1 \
	--trace \

