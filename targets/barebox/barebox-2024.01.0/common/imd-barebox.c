// SPDX-License-Identifier: GPL-2.0-only

#include <common.h>
#include <image-metadata.h>
#include <generated/compile.h>
#include <generated/utsrelease.h>

/*
 * Mark a imd entry as used so that the linker cannot
 * throw it away.
 */
void imd_used(const void *used)
{
}

struct imd_header imd_start_header
__BAREBOX_IMD_SECTION(.barebox_imd_start) = {
	.type = cpu_to_le32(IMD_TYPE_START),
};

struct imd_header imd_end_header
__BAREBOX_IMD_SECTION(.barebox_imd_end) = {
	.type = cpu_to_le32(IMD_TYPE_END),
};

#ifdef CONFIG_IMD_ENDIANNESS
#ifdef CONFIG_CPU_LITTLE_ENDIAN
#define IMD_ENDIANNESS "little"
#else
#define IMD_ENDIANNESS "big"
#endif
BAREBOX_IMD_TAG_STRING(imd_endianness_tag, IMD_TYPE_PARAMETER,
			"endianness=" IMD_ENDIANNESS, 1);
#endif /* CONFIG_IMD_ENDIANNESS */

BAREBOX_IMD_TAG_STRING(imd_build_tag, IMD_TYPE_BUILD, UTS_VERSION, 1);
BAREBOX_IMD_TAG_STRING(imd_release_tag, IMD_TYPE_RELEASE, UTS_RELEASE, 1);
BAREBOX_IMD_TAG_STRING(imd_buildsystem_version_tag, IMD_TYPE_BUILDSYSTEM, BUILDSYSTEM_VERSION, 1);
BAREBOX_IMD_CRC(imd_crc32, 0x0, 1);
