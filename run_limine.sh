kafl fuzz --purge  \
	--image $1 \
        --workdir $2 \
        --seed-dir $3 \
        -p $4 \
	--payload-size 65536 \
	--grimoire -D --radamsa --redqueen --redqueen-hammer \
	--memory 512 \
	--t-hard 1 \
	--t-soft 0.1 \
	--trace \


