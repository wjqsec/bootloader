/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __EFI_EFI_DEVICE_H
#define __EFI_EFI_DEVICE_H

struct efi_device {
	struct device dev;
	efi_guid_t *guids;
	int num_guids;
	efi_handle_t handle;
	efi_handle_t parent_handle;
	void *protocol;
	struct efi_device_path *devpath;
};

struct efi_driver {
	struct driver driver;
	int (*probe)(struct efi_device *efidev);
	void (*remove)(struct efi_device *efidev);
	int (*dev_pause)(struct efi_device *efidev);
	int (*dev_continue)(struct efi_device *efidev);
	efi_guid_t guid;
};

extern struct bus_type efi_bus;

static inline struct efi_device *to_efi_device(struct device *dev)
{
	return container_of(dev, struct efi_device, dev);
}

static inline struct efi_driver *to_efi_driver(struct driver *drv)
{
	return container_of(drv, struct efi_driver, driver);
}

#define device_efi_driver(drv)	\
	register_driver_macro(device, efi, drv)

#define fs_efi_driver(drv)	\
	register_driver_macro(fs, efi, drv)
static inline int efi_driver_register(struct efi_driver *efidrv)
{
	efidrv->driver.bus = &efi_bus;
	return register_driver(&efidrv->driver);
}

int efi_connect_all(void);
void efi_register_devices(void);
struct efi_device *efi_get_bootsource(void);

void efi_pause_devices(void);
void efi_continue_devices(void);

static inline bool efi_device_has_guid(struct efi_device *efidev, efi_guid_t guid)
{
	int i;

	for (i = 0; i < efidev->num_guids; i++) {
		if (!efi_guidcmp(efidev->guids[i], guid))
			return true;
	}

	return false;
}

#endif /* __EFI_EFI_DEVICE_H */
