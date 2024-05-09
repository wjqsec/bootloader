// SPDX-License-Identifier: GPL-2.0-only
/*
 * of_path.c
 *
 * Copyright (c) 2013 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 */

#include <common.h>
#include <malloc.h>
#include <of.h>
#include <fs.h>

#include <linux/mtd/mtd.h>

struct device *of_find_device_by_node_path(const char *path)
{
	struct device *dev;

	for_each_device(dev) {
		if (!dev->of_node)
			continue;
		if (!strcmp(path, dev->of_node->full_name))
			return dev;
	}

	return NULL;
}

/**
 * __of_cdev_find
 *
 * @node: The node to find the cdev for, can be the device or a
 *        partition in the device
 * @part: Optionally, a description of a partition of @node.  See of_find_path
 *
 */
static struct cdev *__of_cdev_find(struct device_node *node, const char *part)
{
	struct device *dev;
	struct cdev *cdev;

	/*
	 * On EFI, where devices are not instantiated from device tree, the
	 * state backend may point at a top-level fixed-partitions partition
	 * subnode with a partuuid property, which will be looked up globally.
	 *
	 * In order to support this binding, we do not early exit when
	 * of_partition_ensure_probed fails, but instead try the custom binding.
	 */
	(void)of_partition_ensure_probed(node);

	dev = of_find_device_by_node_path(node->full_name);
	if (!dev) {
		int ret;
		const char *uuid;
		struct device_node *devnode = node->parent;

		if (of_device_is_compatible(devnode, "fixed-partitions")) {
			devnode = devnode->parent;

			/* when partuuid is specified short-circuit the search for the cdev */
			ret = of_property_read_string(node, "partuuid", &uuid);
			if (!ret)
				return cdev_by_partuuid(uuid) ?: ERR_PTR(-ENODEV);
		}

		dev = of_find_device_by_node_path(devnode->full_name);
		if (!dev)
			return ERR_PTR(-ENODEV);
	}

	if (dev->bus && !dev->driver)
		return ERR_PTR(-EPROBE_DEFER);

	device_detect(dev);

	if (part)
		cdev = device_find_partition(dev, part);
	else
		cdev = cdev_by_device_node(node);

	return cdev ?: ERR_PTR(-ENOENT);
}

/**
 * of_cdev_find
 *
 * @node: The node to find the cdev for, can be the device or a
 *        partition in the device
 *
 */
struct cdev *of_cdev_find(struct device_node *node)
{
	return __of_cdev_find(node, NULL);
}

/**
 * __of_find_path
 *
 * @node: The node to find the cdev for, can be the device or a
 *        partition in the device
 * @part: Optionally, a description of a partition of @node.  See of_find_path
 * @outpath: if this function returns 0 outpath will contain the path belonging
 *           to the input path description. Must be freed with free().
 * @flags: use OF_FIND_PATH_FLAGS_BB to return the .bb device if available
 *
 */
static int __of_find_path(struct device_node *node, const char *part, char **outpath, unsigned flags)
{
	bool add_bb = false;
	struct cdev *cdev;

	cdev = __of_cdev_find(node, part);
	if (IS_ERR(cdev))
		return PTR_ERR(cdev);

	if ((flags & OF_FIND_PATH_FLAGS_BB) && cdev->mtd &&
	    mtd_can_have_bb(cdev->mtd))
		add_bb = true;

	*outpath = basprintf("/dev/%s%s", cdev->name, add_bb ? ".bb" : "");

	return 0;
}

/**
 * of_find_path_by_node - translate a node in the devicetree to a
 *                     	  barebox device path
 *
 * @node: the node we're interested in
 * @outpath: if this function returns 0 outpath will contain the path belonging
 *           to the input path description. Must be freed with free().
 * @flags: use OF_FIND_PATH_FLAGS_BB to return the .bb device if available
 *
 */
int of_find_path_by_node(struct device_node *node, char **outpath, unsigned flags)
{
	return __of_find_path(node, NULL, outpath, flags);
}

/**
 * of_find_node_by_devpath - translate a device path to a device tree node
 *
 * @root: The device tree root. Can be NULL, in this case the internal tree is used
 * @path: The path to look the node up for. Can be "/dev/cdevname" or "cdevname" directly.
 *
 * This is the counterpart of of_find_path_by_node(). Given a path this function tries
 * to find the corresponding node in the given device tree.
 *
 * We first have to find the hardware device in the tree we are passed and then find
 * a partition matching offset/size in this tree. This is necessary because the
 * passed tree may use another partition binding (legacy vs. fixed-partitions). Also
 * the node names may differ (some device trees have partition@<num> instead of
 * partition@<offset>.
 */
struct device_node *of_find_node_by_devpath(struct device_node *root, const char *path)
{
	struct cdev *cdev;
	bool is_partition = false;
	struct device_node *np, *partnode, *rnp;
	loff_t part_offset = 0, part_size = 0;

	pr_debug("%s: looking for path %s\n", __func__, path);

	cdev = cdev_by_name(devpath_to_name(path));
	if (!cdev) {
		pr_debug("%s: cdev %s not found\n", __func__, path);
		return NULL;
	}

	/*
	 * Look for the device node of the master device (the one of_parse_partitions() has
	 * been called with
	 */
	if (cdev->master) {
		is_partition = true;
		if (cdev->mtd)
			part_offset = cdev->mtd->master_offset;
		else
			part_offset = cdev->offset;
		part_size = cdev->size;
		pr_debug("%s path %s: is a partition with offset 0x%08llx, size 0x%08llx\n",
			 __func__, path, part_offset, part_size);
		np = cdev->master->device_node;
	} else {
		np = cdev->device_node;
	}

	/*
	 * Now find the device node of the master device in the device tree we have
	 * been passed.
	 */
	rnp = of_find_node_by_path_from(root, np->full_name);
	if (!rnp) {
		pr_debug("%s path %s: %pOF not found in passed tree\n", __func__, path,
			np);
		return NULL;
	}

	if (!is_partition) {
		pr_debug("%s path %s: returning full device node %pOF\n", __func__, path,
			rnp);
		return rnp;
	}

	/*
	 * Look for a partition with matching offset/size in the device node of
	 * the tree we have been passed.
	 */
	partnode = of_get_child_by_name(rnp, "partitions");
	if (!partnode) {
		pr_debug("%s path %s: using legacy partition binding\n", __func__, path);
		partnode = rnp;
	}

	for_each_child_of_node(partnode, np) {
		const __be32 *reg;
		int na, ns, len;
		loff_t offset, size;

		reg = of_get_property(np, "reg", &len);
		if (!reg)
			return NULL;

		na = of_n_addr_cells(np);
		ns = of_n_size_cells(np);

		if (len < (na + ns) * sizeof(__be32)) {
			pr_err("reg property too small in %pOF\n", np);
			continue;
		}

		offset = of_read_number(reg, na);
		size = of_read_number(reg + na, ns);

		if (part_offset == offset && part_size == size) {
			pr_debug("%s path %s: found matching partition in %pOF\n", __func__, path,
				np);
			return np;
		}
	}

	pr_debug("%s path %s: no matching node found\n", __func__, path);

	return NULL;
}

/**
 * of_find_path - translate a path description in the devicetree to a barebox
 *                path
 *
 * @node: the node containing the property with the path description
 * @propname: the property name of the path description
 * @outpath: if this function returns 0 outpath will contain the path belonging
 *           to the input path description. Must be freed with free().
 * @flags: use OF_FIND_PATH_FLAGS_BB to return the .bb device if available
 *
 * paths in the devicetree have the form of a multistring property. The first
 * string contains the full path to the physical device containing the path or
 * a full path to a partition described by the OF partition binding.
 * The remaining strings have the form "<type>:<options>". Currently supported
 * for <type> are:
 *
 * partname:<partname> - find a partition by its partition name. For mtd
 *                       partitions this is the label. For DOS partitions
 *                       this is the number beginning with 0.
 *
 * examples:
 *
 * device-path = &mmc0, "partname:0";
 * device-path = &norflash, "partname:barebox-environment";
 * device-path = &environment_nor;
 */
int of_find_path(struct device_node *node, const char *propname, char **outpath, unsigned flags)
{
	struct device_node *rnode;
	const char *path;
	const char *part = NULL;
	const char partnamestr[] = "partname:";

	path = of_get_property(node, propname, NULL);
	if (!path)
		return -EINVAL;

	rnode = of_find_node_by_path(path);
	if (!rnode)
		return -ENODEV;

	of_property_read_string_index(node, propname, 1, &part);
	if (part) {
		if (!strncmp(part, partnamestr, sizeof(partnamestr) - 1)) {
			part += sizeof(partnamestr) - 1;
		} else {
			pr_err("Invalid device-path: %s\n", part);
			return -EINVAL;
		}
	}

	return __of_find_path(rnode, part, outpath, flags);
}
