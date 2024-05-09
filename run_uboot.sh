kafl fuzz --purge --bios $1\
	--image ./images/uboot-img \
	-p 1\
	--seed-dir $2 \
	--payload-size 65536 \
	--grimoire -D --radamsa --redqueen --redqueen-hammer \
	--memory 512 \
	--t-hard 1 \
	--t-soft 0.1 \
	--trace \
