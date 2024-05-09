#ifndef __LIBSAIO_GMA_H
#define __LIBSAIO_GMA_H

#include "MacOsVersion.h"
#include "../include/Pci.h"

//#include "device_inject.h"
//#include "../gui/menu_items/menu_items.h"

//XBool setup_gma_devprop(LOADER_ENTRY *Entry, pci_dt_t *gma_dev); // do not use LOADER_ENTRY to avoid dependency
XBool setup_gma_devprop(const MacOsVersion& macOSVersion, const XString8& BuildVersion, EFI_FILE* RootDir, pci_dt_t *gma_dev);

struct gma_gpu_t {
	UINT32 device;
	CONST CHAR8 *name;
};

/*
Chameleon
uint32_t ram = (((getVBEVideoRam() + 512) / 1024) + 512) / 1024;
uint32_t ig_platform_id;
switch (ram)
{
  case 96:
    ig_platform_id = 0x01660000; // 96mb
    break;    
  case 64:
    ig_platform_id = 0x01660009; // 64mb
    break;
  case 32:
    ig_platform_id = 0x01620005; // 32mb
    break;
  default:
*/    

CONST CHAR8
*get_gma_model (
  IN UINT16 DeviceID
  );
    
#endif /* !__LIBSAIO_GMA_H */
