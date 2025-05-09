# SPDX-License-Identifier: GPL-2.0

pbl-firmware-$(CONFIG_FIRMWARE_IMX_LPDDR4_PMU_TRAIN) += \
	lpddr4_pmu_train_1d_dmem.bin \
	lpddr4_pmu_train_1d_imem.bin \
	lpddr4_pmu_train_2d_dmem.bin \
	lpddr4_pmu_train_2d_imem.bin

pbl-firmware-$(CONFIG_FIRMWARE_IMX_DDR4_PMU_TRAIN) += \
	ddr4_dmem_1d.bin \
	ddr4_dmem_2d.bin \
	ddr4_imem_1d.bin \
	ddr4_imem_2d.bin

pbl-firmware-$(CONFIG_FIRMWARE_IMX8MM_ATF) += imx8mm-bl31.bin$(if $(CONFIG_FIRMWARE_IMX8MM_OPTEE),-optee,)
pbl-firmware-$(CONFIG_FIRMWARE_IMX8MN_ATF) += imx8mn-bl31.bin$(if $(CONFIG_FIRMWARE_IMX8MN_OPTEE),-optee,)
pbl-firmware-$(CONFIG_FIRMWARE_IMX8MP_ATF) += imx8mp-bl31.bin$(if $(CONFIG_FIRMWARE_IMX8MP_OPTEE),-optee,)
pbl-firmware-$(CONFIG_FIRMWARE_IMX8MQ_ATF) += imx8mq-bl31.bin
pbl-firmware-$(CONFIG_FIRMWARE_IMX93_ATF) += imx93-bl31.bin
fw-external-$(CONFIG_FIRMWARE_IMX8MM_OPTEE) += imx8mm-bl32.bin
fw-external-$(CONFIG_FIRMWARE_IMX8MN_OPTEE) += imx8mn-bl32.bin
fw-external-$(CONFIG_FIRMWARE_IMX8MP_OPTEE) += imx8mp-bl32.bin
pbl-firmware-$(CONFIG_ARCH_RK3568) += rk3568-bl31.bin
pbl-firmware-$(CONFIG_ARCH_RK3568_OPTEE) += rk3568-op-tee.bin
pbl-firmware-$(CONFIG_ARCH_RK3588) += rk3588-bl31.bin
pbl-firmware-$(CONFIG_ARCH_RK3588_OPTEE) += rk3588-op-tee.bin
pbl-firmware-$(CONFIG_ARCH_RK3399) += rk3399-bl31.bin
pbl-firmware-$(CONFIG_ARCH_RK3399_OPTEE) += rk3399-op-tee.bin
firmware-$(CONFIG_DRIVER_NET_FSL_FMAN) += fsl_fman_ucode_ls1046_r1.0_106_4_18.bin

firmware-$(CONFIG_ARCH_LAYERSCAPE_PPA) += ppa-ls1046a.bin

pbl-firmware-$(CONFIG_FIRMWARE_CCBV2_OPTEE) += ccbv2_optee.bin

# Create $(fwdir) from $(CONFIG_EXTRA_FIRMWARE_DIR) -- if it doesn't have a
# leading /, it's relative to $(srctree).
fwdir := $(subst $(quote),,$(CONFIG_EXTRA_FIRMWARE_DIR))
fwdir := $(addprefix $(srctree)/,$(filter-out /%,$(fwdir)))$(filter /%,$(fwdir))
fwobjdir := $(objtree)/firmware

pbl-y     := $(addsuffix .gen.o, $(pbl-firmware-y))
obj-pbl-y := $(addsuffix .gen.o, $(firmware-y))
pbl-fwext-y := $(addsuffix .extgen.o, $(fw-external-y))

FWNAME    = $(patsubst $(obj)/%.extgen.S,%,$(patsubst $(obj)/%.gen.S,%,$@))
FWSTR     = $(subst /,_,$(subst .,_,$(subst -,_,$(FWNAME))))
FWNAME_EXISTS = $(if $(wildcard $(fwdir)/$(FWNAME)),1,0)

filechk_fwbin = { \
	echo "/* Generated by $(src)/Makefile */"		;\
	echo "\#include <asm-generic/pointer.h>"		;\
	echo ".section .note.GNU-stack,\"\",%progbits"		;\
	echo "    .section $2,\"$3\""				;\
	echo "    .p2align ASM_LGPTR"				;\
	echo ".global _fw_$(FWSTR)_start"			;\
	echo "_fw_$(FWSTR)_start:"				;\
	echo "\#if $(FWNAME_EXISTS)"				;\
	echo "    .incbin \"$(fwdir)/$(FWNAME)\""		;\
	echo "\#else"						;\
	echo "ASM_PTR _fwname_$(FWSTR)"				;\
	echo "\#endif"						;\
	echo ".global _fw_$(FWSTR)_end"				;\
	echo "_fw_$(FWSTR)_end:"				;\
	echo "\#ifdef __PBL__"					;\
	echo "    .section .missing_fw,\"a\""			;\
	echo "_fwname_$(FWSTR):"				;\
	printf '.ascii "%s"\n' 'firmware/$(FWNAME)\n'		;\
	echo "\#endif" 						;\
}

__fwbin_sha = { \
	echo "    .section .rodata.$(FWSTR).sha"		;\
	echo "    .p2align ASM_LGPTR"				;\
	echo ".global _fw_$(FWSTR)_sha_start"			;\
	echo "_fw_$(FWSTR)_sha_start:"				;\
	echo "    .incbin \"$(fwobjdir)/$(FWNAME).sha.bin\""	;\
	echo ".global _fw_$(FWSTR)_sha_end"			;\
	echo "_fw_$(FWSTR)_sha_end:"				;\
}

filechk_fwbin_ext = { \
	$(filechk_fwbin)					;\
	$(__fwbin_sha)						;\
}

$(obj)/%.gen.S: FORCE
	$(call filechk,fwbin,.rodata.$(FWSTR),)

$(obj)/%.extgen.S: $(obj)/%.sha.bin FORCE
	$(call filechk,fwbin_ext,.pblext.$(FWSTR),a)

$(obj)/%.sha.bin: $(obj)/%.sum FORCE
	$(call if_changed,sha256bin)

$(obj)/%.sum: FORCE
	$(if $(wildcard $(fwdir)/$*), $(call if_changed,sha256sum,$(fwdir)/$*), @touch $@)

clean-files += *.sha.bin *.sum

# The .o files depend on the binaries directly; the .S files don't.
$(patsubst %.gen.o,$(obj)/%.gen.o, $(obj-pbl-y)): $(obj)/%.gen.o: $(fwdir)/%

# The same for pbl:
.SECONDEXPANSION:
$(patsubst %.gen.o,$(obj)/%.gen.pbl.o, $(obj-pbl-y) $(pbl-y)): $(obj)/%.gen.pbl.o: $$(wildcard $(fwdir)/%)
$(patsubst %.extgen.o,$(obj)/%.extgen.pbl.o, $(pbl-fwext-y)): $(obj)/%.extgen.pbl.o: $$(wildcard $(fwdir)/%)

pbl-y += $(pbl-fwext-y)

targets := $(patsubst $(obj)/%,%, \
                                $(shell find $(obj) -name \*.gen.S 2>/dev/null))
targets += $(patsubst $(obj)/%,%, \
                                $(shell find $(obj) -name \*.extgen.S 2>/dev/null))

# just to build a built-in.o. Otherwise compilation fails when no
# firmware is built.
obj- += dummy.o
