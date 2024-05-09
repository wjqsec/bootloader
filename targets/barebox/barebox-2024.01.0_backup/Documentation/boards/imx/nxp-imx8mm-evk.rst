NXP i.MX8MM EVK Evaluation Board
================================

Board comes with:

* 2GiB of LPDDR4 RAM
* 16GiB eMMC

Not including booting via serial, the device can boot from either SD or eMMC.

Downloading DDR PHY Firmware
----------------------------

As a part of DDR intialization routine NXP i.MX8MM EVK requires and
uses several binary firmware blobs that are distributed under a
separate EULA and cannot be included in Barebox. In order to obtain
them do the following::

 wget https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/firmware-imx-8.0.bin
 chmod +x firmware-imx-8.0.bin
 ./firmware-imx-8.0.bin

Executing that file should produce a EULA acceptance dialog as well as
result in the following files:

- lpddr4_pmu_train_1d_dmem.bin
- lpddr4_pmu_train_1d_imem.bin
- lpddr4_pmu_train_2d_dmem.bin
- lpddr4_pmu_train_2d_imem.bin

As a last step of this process those files need to be placed in
"firmware/"::

  for f in lpddr4_pmu_train_1d_dmem.bin  \
           lpddr4_pmu_train_1d_imem.bin  \
	   lpddr4_pmu_train_2d_dmem.bin  \
	   lpddr4_pmu_train_2d_imem.bin; \
  do \
	   cp firmware-imx-8.0/firmware/ddr/synopsys/${f} \
	      firmware/${f}; \
  done

DDR Configuration Code
======================

The following two files:

  - arch/arm/boards/nxp-imx8mq-evk/ddr_init.c
  - arch/arm/boards/nxp-imx8mq-evk/ddrphy_train.c

were obtained by running i.MX 8M DDR Tool that can be found here:

https://community.nxp.com/docs/DOC-340179

Only minimal amount of necessary changes were made to those files.
All of the "impedance matching" code is located in "ddr.h".

Build barebox
=============

 make imx_v8_defconfig
 make

Start barebox
=============

The resulting image file is images/barebox-nxp-imx8mm-evk.img. Configure the
board for serial download mode as printed on the PCB. You can start barebox with
the imx-usb-loader tool that comes with barebox like this:

./scripts/imx/imx-usb-loader images/barebox-nxp-imx8mm-evk.img

Installing barebox
==================

When the EVK is strapped to boot from eMMC, the i.MX8M bootrom will
consult the eMMC ext_csd register to determine whether to boot
from the active eMMC boot partition or from the user area.

The same barebox image written to the start of the SD-Card can
be written to the start of the eMMC user area. Power-fail-safe
installation to the eMMC boot partition requires special handling:

  - The barebox image must be written to the inactive boot partition,
    then afterwards, the newly written boot partition is activated
    (This is controlled by the barebox ``mmcX.boot`` variable).

The following steps are required to write the image to the QSPI NOR flash:

  - The 32KiB preamble MMC preamble must be stripped.

  - The QSPI NOR partition ``barebox`` must be erased before the stripped
    image is written. The erase size depends on the stripped image size but
    always start at offset 0.

  - Write the stripped barebox image to the QSPI NOR partition ``barebox``
    at offset 0.

The ``barebox_update`` command takes care of this and need just be
supplied a barebox image as argument.
