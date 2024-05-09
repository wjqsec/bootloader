// SPDX-License-Identifier: GPL-2.0-only
/*
 * bbu.c - barebox update functions
 *
 * Copyright (c) 2012 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 */

#define pr_fmt(fmt) "bbu: " fmt

#include <common.h>
#include <bbu.h>
#include <linux/list.h>
#include <errno.h>
#include <readkey.h>
#include <filetype.h>
#include <libfile.h>
#include <fs.h>
#include <fcntl.h>
#include <malloc.h>
#include <linux/stat.h>
#include <image-metadata.h>
#include <environment.h>
#include <file-list.h>

static LIST_HEAD(bbu_image_handlers);

static void append_bbu_entry(const char *_name,
			     const char *devicefile,
			     struct file_list *files)
{
	char *name;

	name = basprintf("bbu-%s", _name);

	if (file_list_add_entry(files, name, devicefile, 0))
		pr_warn("duplicate partition name %s\n", name);

	free(name);
}

void bbu_append_handlers_to_file_list(struct file_list *files)
{
	struct bbu_handler *handler;

	list_for_each_entry(handler, &bbu_image_handlers, list) {
		const char *cdevname;
		struct stat s;
		char *devpath;

		cdevname = devpath_to_name(handler->devicefile);
		device_detect_by_name(cdevname);

		devpath = basprintf("/dev/%s", cdevname);

		if (stat(devpath, &s) == 0) {
			append_bbu_entry(handler->name, devpath, files);
		} else {
			pr_info("Skipping unavailable handler bbu-%s\n",
				handler->name);
		}

		free(devpath);
	}
}

int bbu_force(struct bbu_data *data, const char *fmt, ...)
{
	va_list args;

	printf("UPDATE: ");

	va_start(args, fmt);

	vprintf(fmt, args);

	va_end(args);

	if (!(data->flags & BBU_FLAG_FORCE))
		goto out;

	if (!data->force)
		goto out;

	data->force--;

	printf(" (forced)\n");

	return 1;
out:
	printf("\n");

	return 0;
}

int bbu_confirm(struct bbu_data *data)
{
	int key;

	if (data->flags & BBU_FLAG_YES)
		return 0;

	if (data->imagefile)
		printf("update barebox from %s using handler %s to %s (y/n)?\n",
			data->imagefile, data->handler_name,
			data->devicefile);
	else
		printf("Refresh barebox on %s using handler %s (y/n)?\n",
			data->devicefile, data->handler_name);

	key = read_key();

	if (key == 'y') {
		printf("updating barebox...\n");
		return 0;
	}

	return -EINTR;
}

struct bbu_handler *bbu_find_handler_by_name(const char *name)
{
	struct bbu_handler *handler;

	list_for_each_entry(handler, &bbu_image_handlers, list) {
		if (!name) {
			if (handler->flags & BBU_HANDLER_FLAG_DEFAULT)
				return handler;
			continue;
		}

		if (!strcmp(handler->name, name))
			return handler;
	}

	return NULL;
}

struct bbu_handler *bbu_find_handler_by_device(const char *devicepath)
{
	struct bbu_handler *handler;
	const char *devname = devpath_to_name(devicepath);

	if (!devicepath)
		return NULL;

	list_for_each_entry(handler, &bbu_image_handlers, list) {
		if (!strcmp(handler->devicefile, devicepath))
			return handler;
	}

	if (devname != devicepath)
		return bbu_find_handler_by_device(devname);

	return NULL;
}

static int bbu_check_of_compat(struct bbu_data *data, unsigned short of_compat_nr)
{
	const struct imd_header *imd = data->imd_data;
	const struct imd_header *of_compat;
	struct device_node *root_node;
	const char *machine, *str;
	int ret;

	if (!IS_ENABLED(CONFIG_OFDEVICE) || !IS_ENABLED(CONFIG_IMD))
		return 0;

	root_node = of_get_root_node();
	if (!root_node)
		return 0;

	if (!of_compat_nr)
		return 0;

	ret = of_property_read_string(root_node, "compatible", &machine);
	if (ret)
		return 0;

	for (; of_compat_nr; of_compat_nr--) {
		of_compat = imd_find_type(imd, IMD_TYPE_OF_COMPATIBLE);
		if (!of_compat)
			return 0;

		str = imd_string_data(of_compat, 0);

		if (of_machine_is_compatible(str)) {
			pr_info("Devicetree compatible \"%s\" matches current machine\n", str);
			return 0;
		}

		pr_debug("machine is incompatible with \"%s\", have \"%s\"\n",
			 str, machine);

		imd = of_compat;
	}

	if (!bbu_force(data, "incompatible machine \"%s\"\n", machine))
		return -EINVAL;

	return 0;
}

static int bbu_check_metadata(struct bbu_data *data)
{
	unsigned short imd_of_compat_nr = 0;
	const struct imd_header *imd;
	int ret;
	char *str;

	if (!IS_ENABLED(CONFIG_IMD))
		return 0;

	if (!data->image)
		return 0;

	data->imd_data = imd_get(data->image, data->len);
	if (IS_ERR(data->imd_data)) {
		data->imd_data = NULL;
		return 0;
	}

	printf("Image Metadata:\n");
	imd_for_each(data->imd_data, imd) {
		uint32_t type = imd_read_type(imd);

		if (imd_read_type(imd) == IMD_TYPE_OF_COMPATIBLE)
			imd_of_compat_nr++;

		if (!imd_is_string(type))
			continue;

		str = imd_concat_strings(imd);

		printf("  %s: %s\n", imd_type_to_name(type), str);
		free(str);
	}

	ret = bbu_check_of_compat(data, imd_of_compat_nr);
	if (ret)
		return ret;

	ret = imd_verify_crc32((void *)data->image, data->len);
	if (ret == -EILSEQ && !(data->flags & BBU_FLAG_FORCE))
		return ret;

	return 0;
}

/*
 * do a barebox update with data from *data
 */
int barebox_update(struct bbu_data *data, struct bbu_handler *handler)
{
	int ret;

	if (!data->image && !data->imagefile &&
	    !(handler->flags & BBU_HANDLER_CAN_REFRESH)) {
		pr_err("No Image file given\n");
		return -EINVAL;
	}

	if (!data->handler_name)
		data->handler_name = handler->name;

	if (!data->devicefile)
		data->devicefile = handler->devicefile;

	ret = bbu_check_metadata(data);
	if (ret)
		return ret;

	ret = handler->handler(handler, data);
	if (ret) {
		printf("update %s\n", (ret == -EINTR) ? "aborted" : "failed");
		return ret;
	}

	printf("update succeeded\n");
	return 0;
}

/*
 * print a list of all registered update handlers
 */
void bbu_handlers_list(void)
{
	struct bbu_handler *handler;

	if (list_empty(&bbu_image_handlers))
		printf("(none)\n");

	list_for_each_entry(handler, &bbu_image_handlers, list)
		printf("%s%-11s -> %-10s\n",
				handler->flags & BBU_HANDLER_FLAG_DEFAULT ?
				"* " : "  ",
				handler->name,
				handler->devicefile);
}

/*
 * register a new update handler
 */
int bbu_register_handler(struct bbu_handler *handler)
{
	if (bbu_find_handler_by_name(handler->name))
		return -EBUSY;

	if (handler->flags & BBU_HANDLER_FLAG_DEFAULT &&
	    bbu_find_handler_by_name(NULL))
		return -EBUSY;

	list_add_tail(&handler->list, &bbu_image_handlers);

	return 0;
}

struct bbu_std {
	struct bbu_handler handler;
	enum filetype filetype;
};

int bbu_mmcboot_handler(struct bbu_handler *handler, struct bbu_data *data,
			int (*chained_handler)(struct bbu_handler *, struct bbu_data *))
{
	struct bbu_data _data = *data;
	int ret;
	char *devicefile = NULL, *bootpartvar = NULL, *bootackvar = NULL;
	const char *bootpart;
	const char *devname = devpath_to_name(data->devicefile);

	ret = device_detect_by_name(devname);
	if (ret) {
		pr_err("Couldn't detect device '%s'\n", devname);
		return ret;
	}

	ret = asprintf(&bootpartvar, "%s.boot", devname);
	if (ret < 0)
		return ret;

	bootpart = getenv(bootpartvar);
	if (!bootpart) {
		ret = -ENOENT;
		goto out;
	}

	if (!strcmp(bootpart, "boot0")) {
		bootpart = "boot1";
	} else {
		bootpart = "boot0";
	}

	ret = asprintf(&devicefile, "/dev/%s.%s", devname, bootpart);
	if (ret < 0)
		goto out;

	_data.devicefile = devicefile;

	ret = chained_handler(handler, &_data);
	if (ret < 0)
		goto out;

	/*
	 * This flag can be set in the chained handler or by
	 * bbu_mmcboot_handler's caller
	 */
	if ((_data.flags | data->flags) & BBU_FLAG_MMC_BOOT_ACK) {
		ret = asprintf(&bootackvar, "%s.boot_ack", devname);
		if (ret < 0)
			goto out;

		ret = setenv(bootackvar, "1");
		if (ret)
			goto out;
	}

	/* on success switch boot source */
	ret = setenv(bootpartvar, bootpart);

out:
	free(bootackvar);
	free(devicefile);
	free(bootpartvar);

	return ret;
}

int bbu_std_file_handler(struct bbu_handler *handler,
			 struct bbu_data *data)
{
	int fd, ret;
	struct stat s;
	unsigned oflags = O_WRONLY;

	device_detect_by_name(devpath_to_name(data->devicefile));

	ret = stat(data->devicefile, &s);
	if (ret) {
		oflags |= O_CREAT;
	} else {
		if (!S_ISREG(s.st_mode) && s.st_size < data->len) {
			printf("Image (%zd) is too big for device (%lld)\n",
					data->len, s.st_size);
		}
	}

	ret = bbu_confirm(data);
	if (ret)
		return ret;

	fd = open(data->devicefile, oflags);
	if (fd < 0)
		return fd;

	ret = protect(fd, data->len, 0, 0);
	if (ret && (ret != -ENOSYS) && (ret != -ENOTSUPP)) {
		printf("unprotecting %s failed with %s\n", data->devicefile,
				strerror(-ret));
		goto err_close;
	}

	ret = erase(fd, data->len, 0);
	if (ret && ret != -ENOSYS) {
		printf("erasing %s failed with %s\n", data->devicefile,
				strerror(-ret));
		goto err_close;
	}

	ret = write_full(fd, data->image, data->len);
	if (ret < 0)
		goto err_close;

	protect(fd, data->len, 0, 1);

	ret = 0;

err_close:
	close(fd);

	return ret;
}

static int bbu_std_file_handler_checked(struct bbu_handler *handler,
					struct bbu_data *data)
{
	struct bbu_std *std = container_of(handler, struct bbu_std, handler);
	enum filetype filetype;

	filetype = file_detect_type(data->image, data->len);
	if (filetype != std->filetype) {
		if (!bbu_force(data, "incorrect image type. Expected: %s, got %s",
				file_type_to_string(std->filetype),
				file_type_to_string(filetype)))
			return -EINVAL;
	}

	return bbu_std_file_handler(handler, data);
}

/**
 * bbu_register_std_file_update() - register a barebox update handler for a
 *                                  standard file-to-device-copy operation
 * @name:	Name of the handler
 * @flags:	BBU_HANDLER_FLAG_* flags
 * @devicefile: the file to write the update image to
 * @imagetype:	The filetype that the update image must have
 *
 * This update handler us suitable for a standard file-to-device copy operation.
 * The handler checks for a filetype and unprotects/erases the device if
 * necessary. If devicefile belongs to a device then the device is checked for
 * enough space before touching it.
 *
 * Return: 0 if successful, negative error code otherwise
 */
int bbu_register_std_file_update(const char *name, unsigned long flags,
		const char *devicefile, enum filetype imagetype)
{
	struct bbu_std *std;
	struct bbu_handler *handler;
	int ret;

	std = xzalloc(sizeof(*std));
	std->filetype = imagetype;

	handler = &std->handler;

	handler->flags = flags;
	handler->devicefile = devicefile;
	handler->name = name;
	handler->handler = bbu_std_file_handler_checked;

	ret = bbu_register_handler(handler);
	if (ret)
		free(std);

	return ret;
}
