// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <stdlib.h>
#include <linux/hw_random.h>

static unsigned int random_seed;

#if RAND_MAX > 32767
#error this rand implementation is for RAND_MAX < 32678 only.
#endif

unsigned int rand(void)
{
	random_seed = random_seed * 1103515245 + 12345;
	return (random_seed / 65536) % (RAND_MAX + 1);
}

void srand(unsigned int seed)
{
	random_seed = seed;
}

/**
 * get_random_bytes - get pseudo random numbers.
 * This interface can be good enough to generate MAC address
 * or use for NAND test.
 */
void get_random_bytes(void *_buf, int len)
{
	char *buf = _buf;

	while (len--)
		*buf++ = rand() % 256;
}

int hwrng_get_crypto_bytes(struct hwrng *rng, void *buf, int len)
{
	while (len) {
		int bytes = hwrng_get_data(rng, buf, len, true);
		if (!bytes)
			return -ENOMEDIUM;

		if (bytes < 0)
			return bytes;

		len -= bytes;
		buf = buf + bytes;
	}

	return 0;
}

/**
 * get_crypto_bytes - get random numbers suitable for cryptographic needs.
 */
int get_crypto_bytes(void *buf, int len)
{
	struct hwrng *rng;
	int err;

	rng = hwrng_get_first();
	err = PTR_ERR_OR_ZERO(rng);
	if (!err) {
		err = hwrng_get_crypto_bytes(rng, buf, len);
		if (!err)
			return 0;
	}

	if (!IS_ENABLED(CONFIG_ALLOW_PRNG_FALLBACK)) {
		pr_err("no HWRNG available!\n");
		return err;
	}

	pr_warn("falling back to Pseudo RNG source!\n");

	get_random_bytes(buf, len);

	return 0;
}
