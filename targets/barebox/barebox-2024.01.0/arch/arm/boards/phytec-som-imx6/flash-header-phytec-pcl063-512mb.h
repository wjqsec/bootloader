/* SPDX-License-Identifier: GPL-2.0-only */

#define SETUP_MDCFG0			\
	wm 32 0x021B000C 0x676B52F3

#define SETUP_MDASP_MDCTL		\
	wm 32 0x021B0040 0x0000004F;	\
	wm 32 0x021B0000 0x84180000

#include "flash-header-phytec-pcl063.h"
