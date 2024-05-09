/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Xilinx Zynq MPSoC Firmware layer
 *
 * Copyright (c) 2018 Thomas Haemmerle <thomas.haemmerle@wolfvision.net>
 *
 * based on Linux xlnx-zynqmp
 *
 *  Michal Simek <michal.simek@xilinx.com>
 *  Davorin Mista <davorin.mista@aggios.com>
 *  Jolly Shah <jollys@xilinx.com>
 *  Rajan Vaja <rajanv@xilinx.com>
 */

#include <common.h>
#include <init.h>
#include <driver.h>
#include <param.h>
#include <linux/arm-smccc.h>

#include <mach/zynqmp/firmware-zynqmp.h>

struct zynqmp_fw {
	struct device *dev;
	u32 ggs[4];
	u32 pggs[4];
};

#define ZYNQMP_TZ_VERSION(MAJOR, MINOR)	((MAJOR << 16) | MINOR)

#define ZYNQMP_PM_VERSION_MAJOR		1
#define ZYNQMP_PM_VERSION_MINOR		0
#define ZYNQMP_TZ_VERSION_MAJOR		1
#define ZYNQMP_TZ_VERSION_MINOR		0

/* SMC SIP service Call Function Identifier Prefix */
#define PM_SIP_SVC		0xC2000000

enum pm_ret_status {
	XST_PM_SUCCESS = 0,
	XST_PM_INTERNAL = 2000,
	XST_PM_CONFLICT,
	XST_PM_NO_ACCESS,
	XST_PM_INVALID_NODE,
	XST_PM_DOUBLE_REQ,
	XST_PM_ABORT_SUSPEND,
};

enum pm_api_id {
	PM_GET_API_VERSION = 1,
	PM_FPGA_LOAD = 22,
	PM_FPGA_GET_STATUS,
	PM_IOCTL = 34,
	PM_QUERY_DATA,
	PM_CLOCK_ENABLE,
	PM_CLOCK_DISABLE,
	PM_CLOCK_GETSTATE,
	PM_CLOCK_SETDIVIDER,
	PM_CLOCK_GETDIVIDER,
	PM_CLOCK_SETRATE,
	PM_CLOCK_GETRATE,
	PM_CLOCK_SETPARENT,
	PM_CLOCK_GETPARENT,
	PM_EFUSE_ACCESS = 53,
	PM_GET_TRUSTZONE_VERSION = 0xA03,
};

/**
 * zynqmp_pm_ret_code() - Convert PMU-FW error codes to Linux error codes
 * @ret_status:		PMUFW return code
 *
 * Return: corresponding Linux error code
 */
static int zynqmp_pm_ret_code(u32 ret_status)
{
	switch (ret_status) {
	case XST_PM_SUCCESS:
	case XST_PM_DOUBLE_REQ:
		return 0;
	case XST_PM_NO_ACCESS:
		return -EACCES;
	case XST_PM_ABORT_SUSPEND:
		return -ECANCELED;
	case XST_PM_INTERNAL:
	case XST_PM_CONFLICT:
	case XST_PM_INVALID_NODE:
	default:
		return -EINVAL;
	}
}

static noinline int do_fw_call_fail(u64 arg0, u64 arg1, u64 arg2,
				    u32 *ret_payload)
{
	return -ENODEV;
}

/*
 * PM function call wrapper
 * Invoke do_fw_call_smc or do_fw_call_hvc, depending on the configuration
 */
static int (*do_fw_call)(u64, u64, u64, u32 *ret_payload) = do_fw_call_fail;

/**
 * do_fw_call_smc() - Call system-level platform management layer (SMC)
 * @arg0:		Argument 0 to SMC call
 * @arg1:		Argument 1 to SMC call
 * @arg2:		Argument 2 to SMC call
 * @ret_payload:	Returned value array
 *
 * Invoke platform management function via SMC call (no hypervisor present).
 *
 * Return: Returns status, either success or error+reason
 */
static noinline int do_fw_call_smc(u64 arg0, u64 arg1, u64 arg2,
				   u32 *ret_payload)
{
	struct arm_smccc_res res;

	arm_smccc_smc(arg0, arg1, arg2, 0, 0, 0, 0, 0, &res);

	if (ret_payload) {
		ret_payload[0] = lower_32_bits(res.a0);
		ret_payload[1] = upper_32_bits(res.a0);
		ret_payload[2] = lower_32_bits(res.a1);
		ret_payload[3] = upper_32_bits(res.a1);
	}

	return zynqmp_pm_ret_code((enum pm_ret_status)res.a0);
}

/**
 * do_fw_call_hvc() - Call system-level platform management layer (HVC)
 * @arg0:		Argument 0 to HVC call
 * @arg1:		Argument 1 to HVC call
 * @arg2:		Argument 2 to HVC call
 * @ret_payload:	Returned value array
 *
 * Invoke platform management function via HVC
 * HVC-based for communication through hypervisor
 * (no direct communication with ATF).
 *
 * Return: Returns status, either success or error+reason
 */
static noinline int do_fw_call_hvc(u64 arg0, u64 arg1, u64 arg2,
				   u32 *ret_payload)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(arg0, arg1, arg2, 0, 0, 0, 0, 0, &res);

	if (ret_payload) {
		ret_payload[0] = lower_32_bits(res.a0);
		ret_payload[1] = upper_32_bits(res.a0);
		ret_payload[2] = lower_32_bits(res.a1);
		ret_payload[3] = upper_32_bits(res.a1);
	}

	return zynqmp_pm_ret_code((enum pm_ret_status)res.a0);
}

/**
 * zynqmp_pm_invoke_fn() - Invoke the system-level platform management layer
 *			   caller function depending on the configuration
 * @pm_api_id:		Requested PM-API call
 * @arg0:		Argument 0 to requested PM-API call
 * @arg1:		Argument 1 to requested PM-API call
 * @arg2:		Argument 2 to requested PM-API call
 * @arg3:		Argument 3 to requested PM-API call
 * @ret_payload:	Returned value array
 *
 * Invoke platform management function for SMC or HVC call, depending on
 * configuration.
 * Following SMC Calling Convention (SMCCC) for SMC64:
 * Pm Function Identifier,
 * PM_SIP_SVC + PM_API_ID =
 *	((SMC_TYPE_FAST << FUNCID_TYPE_SHIFT)
 *	((SMC_64) << FUNCID_CC_SHIFT)
 *	((SIP_START) << FUNCID_OEN_SHIFT)
 *	((PM_API_ID) & FUNCID_NUM_MASK))
 *
 * PM_SIP_SVC	- Registered ZynqMP SIP Service Call.
 * PM_API_ID	- Platform Management API ID.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_invoke_fn(u32 pm_api_id, u32 arg0, u32 arg1,
			u32 arg2, u32 arg3, u32 *ret_payload)
{
	/*
	 * Added SIP service call Function Identifier
	 * Make sure to stay in x0 register
	 */
	u64 smc_arg[4];

	smc_arg[0] = PM_SIP_SVC | pm_api_id;
	smc_arg[1] = ((u64)arg1 << 32) | arg0;
	smc_arg[2] = ((u64)arg3 << 32) | arg2;

	return do_fw_call(smc_arg[0], smc_arg[1], smc_arg[2], ret_payload);
}

static u32 pm_api_version;
static u32 pm_tz_version;

/**
 * zynqmp_pm_get_api_version() - Get version number of PMU PM firmware
 * @version:	Returned version value
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_get_api_version(u32 *version)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!version)
		return -EINVAL;

	/* Check is PM API version already verified */
	if (pm_api_version > 0) {
		*version = pm_api_version;
		return 0;
	}
	ret = zynqmp_pm_invoke_fn(PM_GET_API_VERSION, 0, 0, 0, 0, ret_payload);
	*version = ret_payload[1];

	return ret;
}

/**
 * zynqmp_pm_get_trustzone_version() - Get secure trustzone firmware version
 * @version:	Returned version value
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_get_trustzone_version(u32 *version)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	if (!version)
		return -EINVAL;

	/* Check is PM trustzone version already verified */
	if (pm_tz_version > 0) {
		*version = pm_tz_version;
		return 0;
	}
	ret = zynqmp_pm_invoke_fn(PM_GET_TRUSTZONE_VERSION, 0, 0,
				  0, 0, ret_payload);
	*version = ret_payload[1];

	return ret;
}

/**
 * get_set_conduit_method() - Choose SMC or HVC based communication
 * @np:		Pointer to the device_node structure
 *
 * Use SMC or HVC-based functions to communicate with EL2/EL3.
 *
 * Return: Returns 0 on success or error code
 */
static int get_set_conduit_method(struct device_node *np)
{
	const char *method;

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("%s missing \"method\" property\n", __func__);
		return -ENXIO;
	}

	if (!strcmp("hvc", method)) {
		do_fw_call = do_fw_call_hvc;
	} else if (!strcmp("smc", method)) {
		do_fw_call = do_fw_call_smc;
	} else {
		pr_warn("%s Invalid \"method\" property: %s\n",
			__func__, method);
		return -EINVAL;
	}

	return 0;
}

/**
 * zynqmp_pm_query_data() - Get query data from firmware
 * @qdata:	Variable to the zynqmp_pm_query_data structure
 * @out:	Returned output value
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_query_data(struct zynqmp_pm_query_data qdata, u32 *out)
{
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_QUERY_DATA, qdata.qid, qdata.arg1,
				  qdata.arg2, qdata.arg3, out);

	/*
	 * For clock name query, all bytes in SMC response are clock name
	 * characters and return code is always success. For invalid clocks,
	 * clock name bytes would be zeros.
	 */
	return qdata.qid == PM_QID_CLOCK_GET_NAME ? 0 : ret;
}

/**
 * zynqmp_pm_clock_enable() - Enable the clock for given id
 * @clock_id:	ID of the clock to be enabled
 *
 * This function is used by master to enable the clock
 * including peripherals and PLL clocks.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_enable(u32 clock_id)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_ENABLE, clock_id, 0, 0, 0, NULL);
}

/**
 * zynqmp_pm_clock_disable() - Disable the clock for given id
 * @clock_id:	ID of the clock to be disable
 *
 * This function is used by master to disable the clock
 * including peripherals and PLL clocks.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_disable(u32 clock_id)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_DISABLE, clock_id, 0, 0, 0, NULL);
}

/**
 * zynqmp_pm_clock_getstate() - Get the clock state for given id
 * @clock_id:	ID of the clock to be queried
 * @state:	1/0 (Enabled/Disabled)
 *
 * This function is used by master to get the state of clock
 * including peripherals and PLL clocks.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_getstate(u32 clock_id, u32 *state)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETSTATE, clock_id, 0,
				  0, 0, ret_payload);
	*state = ret_payload[1];

	return ret;
}

/**
 * zynqmp_pm_clock_setdivider() - Set the clock divider for given id
 * @clock_id:	ID of the clock
 * @divider:	divider value
 *
 * This function is used by master to set divider for any clock
 * to achieve desired rate.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_setdivider(u32 clock_id, u32 divider)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_SETDIVIDER, clock_id, divider,
				   0, 0, NULL);
}

/**
 * zynqmp_pm_clock_getdivider() - Get the clock divider for given id
 * @clock_id:	ID of the clock
 * @divider:	divider value
 *
 * This function is used by master to get divider values
 * for any clock.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_getdivider(u32 clock_id, u32 *divider)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETDIVIDER, clock_id, 0,
				  0, 0, ret_payload);
	*divider = ret_payload[1];

	return ret;
}

/**
 * zynqmp_pm_clock_setrate() - Set the clock rate for given id
 * @clock_id:	ID of the clock
 * @rate:	rate value in hz
 *
 * This function is used by master to set rate for any clock.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_setrate(u32 clock_id, u64 rate)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_SETRATE, clock_id,
				   lower_32_bits(rate),
				   upper_32_bits(rate),
				   0, NULL);
}

/**
 * zynqmp_pm_clock_getrate() - Get the clock rate for given id
 * @clock_id:	ID of the clock
 * @rate:	rate value in hz
 *
 * This function is used by master to get rate
 * for any clock.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_getrate(u32 clock_id, u64 *rate)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETRATE, clock_id, 0,
				  0, 0, ret_payload);
	*rate = ((u64)ret_payload[2] << 32) | ret_payload[1];

	return ret;
}

/**
 * zynqmp_pm_clock_setparent() - Set the clock parent for given id
 * @clock_id:	ID of the clock
 * @parent_id:	parent id
 *
 * This function is used by master to set parent for any clock.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_setparent(u32 clock_id, u32 parent_id)
{
	return zynqmp_pm_invoke_fn(PM_CLOCK_SETPARENT, clock_id,
				   parent_id, 0, 0, NULL);
}

/**
 * zynqmp_pm_clock_getparent() - Get the clock parent for given id
 * @clock_id:	ID of the clock
 * @parent_id:	parent id
 *
 * This function is used by master to get parent index
 * for any clock.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_clock_getparent(u32 clock_id, u32 *parent_id)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret;

	ret = zynqmp_pm_invoke_fn(PM_CLOCK_GETPARENT, clock_id, 0,
				  0, 0, ret_payload);
	*parent_id = ret_payload[1];

	return ret;
}

/**
 * zynqmp_is_valid_ioctl() - Check whether IOCTL ID is valid or not
 * @ioctl_id:	IOCTL ID
 *
 * Return: 1 if IOCTL is valid else 0
 */
static inline int zynqmp_is_valid_ioctl(u32 ioctl_id)
{
	switch (ioctl_id) {
	case IOCTL_SET_PLL_FRAC_MODE:
	case IOCTL_GET_PLL_FRAC_MODE:
	case IOCTL_SET_PLL_FRAC_DATA:
	case IOCTL_GET_PLL_FRAC_DATA:
		return 1;
	default:
		return 0;
	}
}

/**
 * zynqmp_pm_ioctl() - PM IOCTL API for device control and configs
 * @node_id:	Node ID of the device
 * @ioctl_id:	ID of the requested IOCTL
 * @arg1:	Argument 1 to requested IOCTL call
 * @arg2:	Argument 2 to requested IOCTL call
 * @out:	Returned output value
 *
 * This function calls IOCTL to firmware for device control and configuration.
 *
 * Return: Returns status, either success or error+reason
 */
static int zynqmp_pm_ioctl(u32 node_id, u32 ioctl_id, u32 arg1, u32 arg2,
			   u32 *out)
{
	if (!zynqmp_is_valid_ioctl(ioctl_id))
		return -EINVAL;

	return zynqmp_pm_invoke_fn(PM_IOCTL, node_id, ioctl_id,
				   arg1, arg2, out);
}

/*
 * zynqmp_pm_write_ggs() - PM API for writing global general storage (ggs)
 * @index:	GGS register index
 * @value:	Register value to be written
 *
 * This function writes value to GGS register.
 *
 * Return:      Returns status, either success or error+reason
 */
int zynqmp_pm_write_ggs(u32 index, u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_WRITE_GGS,
				   index, value, NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_write_ggs);

/**
 * zynqmp_pm_read_ggs() - PM API for reading global general storage (ggs)
 * @index:	GGS register index
 * @value:	Register value to be written
 *
 * This function returns GGS register value.
 *
 * Return:	Returns status, either success or error+reason
 */
int zynqmp_pm_read_ggs(u32 index, u32 *value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_READ_GGS,
				   index, 0, value);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_read_ggs);

/**
 * zynqmp_pm_write_pggs() - PM API for writing persistent global general
 *			     storage (pggs)
 * @index:	PGGS register index
 * @value:	Register value to be written
 *
 * This function writes value to PGGS register.
 *
 * Return:	Returns status, either success or error+reason
 */
int zynqmp_pm_write_pggs(u32 index, u32 value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_WRITE_PGGS, index, value,
				   NULL);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_write_pggs);

/**
 * zynqmp_pm_read_pggs() - PM API for reading persistent global general
 *			     storage (pggs)
 * @index:	PGGS register index
 * @value:	Register value to be written
 *
 * This function returns PGGS register value.
 *
 * Return:	Returns status, either success or error+reason
 */
int zynqmp_pm_read_pggs(u32 index, u32 *value)
{
	return zynqmp_pm_invoke_fn(PM_IOCTL, 0, IOCTL_READ_PGGS, index, 0,
				   value);
}
EXPORT_SYMBOL_GPL(zynqmp_pm_read_pggs);

/**
 * zynqmp_pm_fpga_load - Perform the fpga load
 * @address:	Address to write to
 * @size:	pl bitstream size
 * @flags:	Flags are used to specify the type of Bitstream file -
 * 		defined in ZYNQMP_FPGA_BIT_*-macros
 *
 * This function provides access to xilfpga library to transfer
 * the required bitstream into PL.
 *
 * Return:	Returns status, either success or error+reason
 */
static int zynqmp_pm_fpga_load(u64 address, u32 size, u32 flags)
{
	if (!address || !size)
		return -EINVAL;

	return zynqmp_pm_invoke_fn(PM_FPGA_LOAD,
			lower_32_bits(address),	upper_32_bits(address),
			size, flags, NULL);
}

/**
 * zynqmp_pm_fpga_get_status - Read value from PCAP status register
 * @value:	Value to read
 *
 * This function provides access to the xilfpga library to get
 * the PCAP status
 *
 * Return:	Returns status, either success or error+reason
 */
static int zynqmp_pm_fpga_get_status(u32 *value)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	int ret = 0;

	if (!value)
		return -EINVAL;

	ret = zynqmp_pm_invoke_fn(PM_FPGA_GET_STATUS, 0, 0, 0, 0, ret_payload);
	*value = ret_payload[1];

	return ret;
}

static const struct zynqmp_eemi_ops eemi_ops = {
	.get_api_version = zynqmp_pm_get_api_version,
	.query_data = zynqmp_pm_query_data,
	.clock_enable = zynqmp_pm_clock_enable,
	.clock_disable = zynqmp_pm_clock_disable,
	.clock_getstate = zynqmp_pm_clock_getstate,
	.clock_setdivider = zynqmp_pm_clock_setdivider,
	.clock_getdivider = zynqmp_pm_clock_getdivider,
	.clock_setrate = zynqmp_pm_clock_setrate,
	.clock_getrate = zynqmp_pm_clock_getrate,
	.clock_setparent = zynqmp_pm_clock_setparent,
	.clock_getparent = zynqmp_pm_clock_getparent,
	.ioctl = zynqmp_pm_ioctl,
	.fpga_getstatus = zynqmp_pm_fpga_get_status,
	.fpga_load = zynqmp_pm_fpga_load,
};

/**
 * zynqmp_pm_get_eemi_ops - Get eemi ops functions
 *
 * Return: Pointer of eemi_ops structure
 */
const struct zynqmp_eemi_ops *zynqmp_pm_get_eemi_ops(void)
{
	return &eemi_ops;
}
EXPORT_SYMBOL_GPL(zynqmp_pm_get_eemi_ops);

static bool parse_reg(const char *reg, unsigned *idx)
{
	bool pggs = reg[0] == 'p';
	kstrtouint(reg + pggs + sizeof("ggs") - 1, 10, idx);
	return pggs;
}

static int ggs_set(struct param_d *p, void *_val)
{
	u32 *val = _val;
	unsigned idx;

	if (parse_reg(p->name, &idx))
		return zynqmp_pm_write_pggs(idx, *val);
	else
		return zynqmp_pm_write_ggs(idx, *val);
}
static int ggs_get(struct param_d *p, void *_val)
{
	u32 ret_payload[PAYLOAD_ARG_CNT];
	u32 *val = _val;
	unsigned idx;
	int ret;

	if (parse_reg(p->name, &idx))
		ret = zynqmp_pm_read_pggs(idx, ret_payload);
	else
		ret = zynqmp_pm_read_ggs(idx, ret_payload);

	if (ret)
		return ret;

	*val = ret_payload[1];

	return 0;
}

static inline void dev_add_param_ggs(struct zynqmp_fw *fw, const char *str, u32 *value)
{
	dev_add_param_uint32(fw->dev, str, ggs_set, ggs_get, value, "0x%x", value);
}

static int zynqmp_firmware_probe(struct device *dev)
{

	struct zynqmp_fw *fw;
	int ret;

	fw = xzalloc(sizeof(*fw));

	dev_add_alias(dev, "zynqmp_fw");

	ret = get_set_conduit_method(dev->of_node);
	if (ret)
		goto out;

	zynqmp_pm_get_api_version(&pm_api_version);
	if (pm_api_version < ZYNQMP_PM_VERSION(ZYNQMP_PM_VERSION_MAJOR,
					       ZYNQMP_PM_VERSION_MINOR)) {
		dev_err(dev, "Platform Management API version error."
				"Expected: v%d.%d - Found: v%d.%d\n",
				ZYNQMP_PM_VERSION_MAJOR,
				ZYNQMP_PM_VERSION_MINOR,
				pm_api_version >> 16, pm_api_version & 0xFFFF);
		ret = -EIO;
		goto out;
	}
	dev_dbg(dev, "Platform Management API v%d.%d\n",
			pm_api_version >> 16, pm_api_version & 0xFFFF);

	ret = zynqmp_pm_get_trustzone_version(&pm_tz_version);
	if (ret) {
		dev_err(dev, "Legacy trustzone found without version support\n");
		ret = -EIO;
		goto out;
	}

	if (pm_tz_version < ZYNQMP_TZ_VERSION(ZYNQMP_TZ_VERSION_MAJOR,
					      ZYNQMP_TZ_VERSION_MINOR)) {
		dev_err(dev, "Trustzone version error."
				"Expected: v%d.%d - Found: v%d.%d\n",
				ZYNQMP_TZ_VERSION_MAJOR,
				ZYNQMP_TZ_VERSION_MINOR,
				pm_tz_version >> 16, pm_tz_version & 0xFFFF);
		ret = -EIO;
		goto out;
	}
	dev_dbg(dev, "Trustzone version v%d.%d\n",
			pm_tz_version >> 16, pm_tz_version & 0xFFFF);

	of_platform_populate(dev->of_node, NULL, dev);

	fw->dev = dev;

	dev_add_param_ggs(fw, "ggs0", &fw->ggs[0]);
	dev_add_param_ggs(fw, "ggs1", &fw->ggs[1]);
	dev_add_param_ggs(fw, "ggs2", &fw->ggs[2]);
	dev_add_param_ggs(fw, "ggs3", &fw->ggs[3]);

	dev_add_param_ggs(fw, "pggs0", &fw->pggs[0]);
	dev_add_param_ggs(fw, "pggs1", &fw->pggs[1]);
	dev_add_param_ggs(fw, "pggs2", &fw->pggs[2]);
	dev_add_param_ggs(fw, "pggs3", &fw->pggs[3]);
out:
	if (ret)
		do_fw_call = do_fw_call_fail;
	return ret;
}

static struct of_device_id zynqmp_firmware_id_table[] = {
	{ .compatible = "xlnx,zynqmp-firmware", },
	{}
};
MODULE_DEVICE_TABLE(of, zynqmp_firmware_id_table);

static struct driver zynqmp_firmware_driver = {
	.name = "zynqmp_firmware",
	.probe = zynqmp_firmware_probe,
	.of_compatible = DRV_OF_COMPAT(zynqmp_firmware_id_table),
};

core_platform_driver(zynqmp_firmware_driver);
