/*
 *	Copyright 2009 Jasmin Fazlic All rights reserved.
 *
 *
 *	Cleaned and merged by iNDi
 */
// UEFI adaptation by usr-sse2, then slice, dmazar, jief



#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile
#include "device_inject.h"
#include "FixBiosDsdt.h"
#include "../include/Devices.h"
#include "../refit/lib.h"
#include "../Platform/Settings.h"

#ifndef DEBUG_INJECT
#ifndef DEBUG_ALL
#define DEBUG_INJECT 1
#else
#define DEBUG_INJECT DEBUG_ALL
#endif
#endif

#if DEBUG_INJECT == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_INJECT, __VA_ARGS__)
#endif

#define MAX_NUM_DEVICES  64

UINT32 devices_number = 1;
UINT32 builtin_set    = 0;
DevPropString *device_inject_string = NULL;
UINT8  *device_inject_stringdata    = NULL;
UINT32 device_inject_stringlength   = 0;

//pci_dt_t* nvdevice;
//SwapBytes16 or 32
/*
 static UINT16 dp_swap16(UINT16 toswap)
 {
 return (((toswap & 0x00FF) << 8) | ((toswap & 0xFF00) >> 8));
 }

 static UINT32 dp_swap32(UINT32 toswap)
 {
 return  ((toswap & 0x000000FF) << 24) |
 ((toswap & 0x0000FF00) << 8 ) |
 ((toswap & 0x00FF0000) >> 8 ) |
 ((toswap & 0xFF000000) >> 24);
 }
 */

DevPropString *devprop_create_string(void)
{
  //	DBG("Begin creating strings for devices:\n");
  device_inject_string = (DevPropString*)AllocateZeroPool(sizeof(DevPropString));

  if(device_inject_string == NULL)
    return NULL;

  device_inject_string->length = 12;
  device_inject_string->WHAT2 = 0x01000000;
  return device_inject_string;
}


XString8 get_pci_dev_path(pci_dt_t *PciDt)
{
  //	DBG("get_pci_dev_path");
  XString8  returnValue;
  XStringW	devpathstr;
  EFI_DEVICE_PATH_PROTOCOL*	DevicePath = NULL;

  DevicePath = DevicePathFromHandle(PciDt->DeviceHandle);
  if (!DevicePath)
    return NullXString8;
  returnValue = FileDevicePathToXStringW(DevicePath);
  return returnValue;
}

UINT32 pci_config_read32(pci_dt_t *PciDt, UINT8 reg)
{
  //	DBG("pci_config_read32\n");
  EFI_STATUS Status;
  EFI_PCI_IO_PROTOCOL		*PciIo;
  PCI_TYPE00				Pci;
  UINT32					res;

  //somehow definition for gBS->OpenProtocol() is different from other parts of the project
  Status = gBS->OpenProtocol(PciDt->DeviceHandle, gEfiPciIoProtocolGuid, (void**)&PciIo, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(Status)){
    DBG("pci_config_read cant open protocol\n");
    return 0;
  }
  Status = PciIo->Pci.Read(PciIo,EfiPciIoWidthUint32, 0, sizeof(Pci) / sizeof(UINT32), &Pci);
  if (EFI_ERROR(Status)) {
    DBG("pci_config_read cant read pci\n");
    return 0;
  }
  Status = PciIo->Pci.Read (
                            PciIo,
                            EfiPciIoWidthUint32,
                            (UINT32)(reg & ~3),
                            1,
                            &res
                            );
  if (EFI_ERROR(Status)) {
    DBG("pci_config_read32 failed %s\n", efiStrError(Status));
    return 0;
  }
  return res;
}

bool SameDevice(DevPropDevice* D1, DevPropDevice* D2)
{
//  DBG("paths 1=%u 2=%u\n", D1->num_pci_devpaths, D2->num_pci_devpaths);
  if (D1->num_pci_devpaths != D2->num_pci_devpaths) return false;
  for (UINT32 x=0; x < D1->num_pci_devpaths; x++) {
//    DBG("  funcs 1=%u 2=%u\n", D1->pci_dev_path[x].function, D2->pci_dev_path[x].function);
    if (D1->pci_dev_path[x].function != D2->pci_dev_path[x].function) return false;
//    DBG("  devs 1=%u 2=%u\n", D1->pci_dev_path[x].device, D2->pci_dev_path[x].device);
    if (D1->pci_dev_path[x].device != D2->pci_dev_path[x].device) return false;
  }
  DBG("found same device\n");
  return true;
}


//UINT32 PciAddrFromDevicePath(EFI_DEVICE_PATH_PROTOCOL* DevicePath)
//{
//	return 0;
//}
//Size = GetDevicePathSize (DevicePath);

//dmazar: replaced by devprop_add_device_pci

DevPropDevice *devprop_add_device_pci(DevPropString *StringBuf, pci_dt_t *PciDt, EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
//  EFI_DEVICE_PATH_PROTOCOL		*DevicePath;
  DevPropDevice               *device;
  UINT32                      NumPaths;

  if (StringBuf == NULL /* || PciDt == NULL */) {
    return NULL;
  }

  //просмотреть StringBuf->entries[] в поисках такого же девайса
  //SameDevice(DevPropDevice* D1, DevPropDevice* D2)

  if (!DevicePath && (PciDt != 0)) {
    DevicePath = DevicePathFromHandle(PciDt->DeviceHandle);
  }
  // 	DBG("devprop_add_device_pci %ls ", DevicePathToStr(DevicePath));
  if (!DevicePath)
    return NULL;

  device = (__typeof__(device))AllocateZeroPool(sizeof(DevPropDevice));
  if (!device) {
    return NULL;
  }

  device->numentries = 0x00;

  //
  // check and copy ACPI_DEVICE_PATH
  //
  if (DevicePath->Type == ACPI_DEVICE_PATH && DevicePath->SubType == ACPI_DP) {
    //		CopyMem(&device->acpi_dev_path, DevicePath, sizeof(struct ACPIDevPath));
    device->acpi_dev_path.length = 0x0c;
    device->acpi_dev_path.type = 0x02;
    device->acpi_dev_path.subtype = 0x01;
    device->acpi_dev_path._HID = 0x0a0341d0;
    device->acpi_dev_path._UID = (((ACPI_HID_DEVICE_PATH*)DevicePath)->UID)?0x80:0;

//    		DBG("ACPI HID=%X, UID=%X ", device->acpi_dev_path._HID, device->acpi_dev_path._UID);
  } else {
//    		DBG("not ACPI\n");
    FreePool(device);
    return NULL;
  }

  //
  // copy PCI paths
  //
  for (NumPaths = 0; NumPaths < MAX_PCI_DEV_PATHS; NumPaths++) {
    DevicePath = NextDevicePathNode(DevicePath);
    if (DevicePath->Type == HARDWARE_DEVICE_PATH && DevicePath->SubType == HW_PCI_DP) {
      CopyMem(&device->pci_dev_path[NumPaths], DevicePath, sizeof(struct PCIDevPath));
 //   	DBG("PCI[%d] f=%X, d=%X ", NumPaths, device->pci_dev_path[NumPaths].function, device->pci_dev_path[NumPaths].device);
    } else {
      // not PCI path - break the loop
      break;
    }
  }

  if (NumPaths == 0) {
//    DBG("NumPaths == 0 \n");
    FreePool(device);
    return NULL;
  }

//  DBG("-> NumPaths=%d\n", NumPaths);
  device->num_pci_devpaths = (UINT8)NumPaths;
  device->length = (UINT32)(24U + (6U * NumPaths));

  device->path_end.length = 0x04;
  device->path_end.type = 0x7f;
  device->path_end.subtype = 0xff;

  device->string = StringBuf;
  device->data = NULL;


  if(!StringBuf->entries) {
    StringBuf->entries = (DevPropDevice**)AllocateZeroPool(MAX_NUM_DEVICES * sizeof(device));
    if(!StringBuf->entries) {
      FreePool(device);
      return NULL;
    }
  }

  DevPropDevice* D1;
  for (int i=0; i<StringBuf->numentries; i++) {
    D1 = StringBuf->entries[i];
    if (SameDevice(D1, device)) {
      FreePool(device);
      return D1;
    }
  }

  StringBuf->length += device->length;
  StringBuf->entries[StringBuf->numentries++] = device;

  return device;
}



XBool devprop_add_value(DevPropDevice *device, CONST CHAR8 *nm, const UINT8 *vl, UINTN len)
{
  UINT32 offset;
  UINT32 off;
  UINT32 length;
  UINT8 *data;
  UINTN i, l;
  UINT32 *datalength;
  UINT8 *newdata;

  if(!device || !nm || !vl /*|| !len*/) //rehabman: allow zero length data
    return false;
  /*	DBG("devprop_add_value %s=", nm);
   for (i=0; i<len; i++) {
   DBG("%02X", vl[i]);
   }
   DBG("\n"); */
  l = AsciiStrLen(nm);
  length = (UINT32)((l * 2) + len + (2 * sizeof(UINT32)) + 2);
  data = (UINT8*)AllocateZeroPool(length);
  if(!data)
    return false;

  off= 0;

  data[off+1] = (UINT8)(((l * 2) + 6) >> 8);
  data[off] =   ((l * 2) + 6) & 0x00FF;

  off += 4;

  for(i = 0 ; i < l ; i++, off += 2) {
    data[off] = *nm++;
  }

  off += 2;
  l = len;
  datalength = (UINT32*)&data[off];
  *datalength = (UINT32)(l + 4);
  off += 4;
  for(i = 0 ; i < l ; i++, off++) {
    data[off] = *vl++;
  }

  offset = device->length - (24 + (6 * device->num_pci_devpaths));

  newdata = (UINT8*)AllocateZeroPool((length + offset));
  if(!newdata) {
    FreePool(data);
    return false;
  }
  if((device->data) && (offset > 1)) {
 		CopyMem((void*)newdata, (void*)device->data, offset);
  }

  CopyMem((void*)(newdata + offset), (void*)data, length);

  device->length += length;
  device->string->length += length;
  device->numentries++;
  device->data = newdata;

  FreePool(data);
  return true;
}

XBool devprop_add_value(DevPropDevice *device, const XString8& nm, const XBuffer<uint8_t>& vl)
{
  return devprop_add_value(device, nm.data(), vl.data(), vl.size());
}

XBuffer<char> devprop_generate_string(DevPropString *StringBuf)
{
  UINTN len = StringBuf->length * 2;

  XBuffer<char> buffer;
  buffer.dataSized(len+1);

//  DbgHeader("Devprop Generate String");

  buffer.S8Catf("%08X%08X%04hX%04hX", SwapBytes32(StringBuf->length), StringBuf->WHAT2, SwapBytes16(StringBuf->numentries), StringBuf->WHAT3);
  for (int i = 0; i < StringBuf->numentries; i++) {
    UINT8 *dataptr = StringBuf->entries[i]->data;
    if (!dataptr) continue;
    buffer.S8Catf("%08X%04hX%04hX", SwapBytes32(StringBuf->entries[i]->length),
                SwapBytes16(StringBuf->entries[i]->numentries), StringBuf->entries[i]->WHAT2); //FIXME: wrong buffer sizes!

    buffer.S8Catf("%02hhX%02hhX%04hX%08X%08X", StringBuf->entries[i]->acpi_dev_path.type,
                StringBuf->entries[i]->acpi_dev_path.subtype,
                SwapBytes16(StringBuf->entries[i]->acpi_dev_path.length),
                SwapBytes32(StringBuf->entries[i]->acpi_dev_path._HID),
                SwapBytes32(StringBuf->entries[i]->acpi_dev_path._UID));

    for(int x = 0; x < StringBuf->entries[i]->num_pci_devpaths; x++) {
      buffer.S8Catf("%02hhX%02hhX%04hX%02hhX%02hhX", StringBuf->entries[i]->pci_dev_path[x].type,
                  StringBuf->entries[i]->pci_dev_path[x].subtype,
                  SwapBytes16(StringBuf->entries[i]->pci_dev_path[x].length),
                  StringBuf->entries[i]->pci_dev_path[x].function,
                  StringBuf->entries[i]->pci_dev_path[x].device);
    }

    buffer.S8Catf("%02hhX%02hhX%04hX", StringBuf->entries[i]->path_end.type,
                StringBuf->entries[i]->path_end.subtype,
                SwapBytes16(StringBuf->entries[i]->path_end.length));

    for(UINT32 x = 0; x < (StringBuf->entries[i]->length) - (24 + (6 * StringBuf->entries[i]->num_pci_devpaths)); x++) {
      buffer.S8Catf("%02hhX", *dataptr++);
    }

  }
//  DBG("string=%s\n", buffer.data());
  return buffer;
}


void devprop_free_string()
{
  INT32 i;
  if(!device_inject_string)
    return;

  for(i = 0; i < device_inject_string->numentries; i++) {
    if(device_inject_string->entries[i]) {
      if(device_inject_string->entries[i]->data) {
        FreePool(device_inject_string->entries[i]->data);
      }
    }
  }
  FreePool(device_inject_string->entries);
  FreePool(device_inject_string);
  device_inject_string = NULL;
}

static UINT8   builtin = 0x0;

// Ethernet built-in device injection
XBool set_eth_props(pci_dt_t *eth_dev)
{
#if DEBUG_INJECT
//  CHAR8           *devicepath;
#endif
  DevPropDevice   *device = NULL;
  XBool           Injected = false;
  UINTN           i;
  CHAR8           compatible[64];
  
  if (!gSettings.Devices.LANInjection) {
    return false;
  }

  if (!device_inject_string) {
    device_inject_string = devprop_create_string();
  }
#if DEBUG_INJECT
//  devicepath = get_pci_dev_path(eth_dev);
#endif
  if (eth_dev && !eth_dev->used) {
    device = devprop_add_device_pci(device_inject_string, eth_dev, NULL);
    eth_dev->used = true;
  }

  if (!device) {
    return false;
  }
  // -------------------------------------------------
//  DBG("LAN Controller [%04X:%04X] :: %s\n", eth_dev->vendor_id, eth_dev->device_id, devicepath);
  if (eth_dev->vendor_id != 0x168c && builtin_set == 0) {
 		builtin_set = 1;
 		builtin = 0x01;
  }

  if (gSettings.Devices.AddPropertyArray.size() != 0xFFFE) { // Looks like NrAddProperties == 0xFFFE is not used anymore
    for (i = 0; i < gSettings.Devices.AddPropertyArray.size(); i++) {
      if (gSettings.Devices.AddPropertyArray[i].Device != DEV_LAN) {
        continue;
      }
      Injected = true;

      if (!gSettings.Devices.AddPropertyArray[i].MenuItem.BValue) {
        //DBG("  disabled property Key: %s, len: %d\n", gSettings.Devices.AddPropertyArray[i].Key, gSettings.Devices.AddPropertyArray[i].ValueLen);
      } else {
        devprop_add_value(device, gSettings.Devices.AddPropertyArray[i].Key, gSettings.Devices.AddPropertyArray[i].Value);
        //DBG("  added property Key: %s, len: %d\n", gSettings.Devices.AddPropertyArray[i].Key, gSettings.Devices.AddPropertyArray[i].ValueLen);
      }
    }
  }
  if (Injected) {
    DBG("custom LAN properties injected, continue\n");
    //    return true;
  }

  //  DBG("Setting dev.prop built-in=0x%X\n", builtin);
//  devprop_add_value(device, "device_type", (UINT8*)"Ethernet", 9);
  if (gSettings.Devices.FakeID.FakeLAN) {
    UINT32 FakeID = gSettings.Devices.FakeID.FakeLAN >> 16;
    devprop_add_value(device, "device-id", (UINT8*)&FakeID, 4);
    snprintf(compatible, 64, "pci%x,%x", (gSettings.Devices.FakeID.FakeLAN & 0xFFFF), FakeID);
    devprop_add_value(device, "compatible", (UINT8*)&compatible[0], 12);
    FakeID = gSettings.Devices.FakeID.FakeLAN & 0xFFFF;
    devprop_add_value(device, "vendor-id", (UINT8*)&FakeID, 4);
  }
  else if (eth_dev->vendor_id == 0x11AB && eth_dev->device_id == 0x4364)
  {
      UINT32 FakeID = 0x4354;
      devprop_add_value(device, "device-id", (UINT8*)&FakeID, 4);
  }


  return devprop_add_value(device, "built-in", (UINT8*)&builtin, 1);
}

#define PCI_IF_XHCI 0x30

static UINT8   clock_id = 0;
static UINT16  current_available = 1200; //mA
static UINT16  current_extra     = 700;
static UINT16  current_in_sleep  = 1000;
static UINT16  current_available_high = 2100; //mA
static UINT16  current_extra_high    = 3200;

XBool set_usb_props(pci_dt_t *usb_dev)
{
#if DEBUG_INJECT
//  CHAR8           *devicepath;
#endif
  DevPropDevice   *device = NULL;
  UINT32          fake_devid;
  XBool           Injected = false;
  UINTN           i;

  if (!device_inject_string)
    device_inject_string = devprop_create_string();
#if DEBUG_INJECT
//  devicepath = get_pci_dev_path(usb_dev);
#endif

  if (usb_dev && !usb_dev->used) {
    device = devprop_add_device_pci(device_inject_string, usb_dev, NULL);
    usb_dev->used = true;
  }

  if (!device) {
    return false;
  }
 // -------------------------------------------------
 // DBG("USB Controller [%04X:%04X] :: %s\n", usb_dev->vendor_id, usb_dev->device_id, devicepath);
 // DBG("Setting dev.prop built-in=0x%X\n", builtin);

  if (gSettings.Devices.AddPropertyArray.size() != 0xFFFE) { // Looks like NrAddProperties == 0xFFFE is not used anymore
    for (i = 0; i < gSettings.Devices.AddPropertyArray.size(); i++) {
      if (gSettings.Devices.AddPropertyArray[i].Device != DEV_USB) {
        continue;
      }
      Injected = true;

      if (!gSettings.Devices.AddPropertyArray[i].MenuItem.BValue) {
        //DBG("  disabled property Key: %s, len: %d\n", gSettings.Devices.AddPropertyArray[i].Key, gSettings.Devices.AddPropertyArray[i].ValueLen);
      } else {
        devprop_add_value(device, gSettings.Devices.AddPropertyArray[i].Key, gSettings.Devices.AddPropertyArray[i].Value);
        //DBG("  added property Key: %s, len: %d\n", gSettings.Devices.AddPropertyArray[i].Key, gSettings.Devices.AddPropertyArray[i].ValueLen);
      }
    }
  }
  if (Injected) {
    DBG("custom USB properties injected, continue\n");
    //    return true;
  }

  if (gSettings.Devices.USB.InjectClockID) {
    devprop_add_value(device, "AAPL,clock-id", (UINT8*)&clock_id, 1);
    clock_id++;
  }

  fake_devid = usb_dev->device_id & 0xFFFF;
  if ((fake_devid & 0xFF00) == 0x2900) {
//    fake_devid &= 0xFEFF;
    fake_devid &= ~0xFF00;
    fake_devid |= 0x3A00;
    devprop_add_value(device, "device-id", (UINT8*)&fake_devid, 4);
  }
  switch (usb_dev->subclass) {
    case PCI_IF_UHCI:
      devprop_add_value(device, "device_type", (UINT8*)"UHCI", 4);
      break;
    case PCI_IF_OHCI:
      devprop_add_value(device, "device_type", (UINT8*)"OHCI", 4);
      break;
    case PCI_IF_EHCI:
      devprop_add_value(device, "device_type", (UINT8*)"EHCI", 4);
      if (gSettings.Devices.USB.HighCurrent) {
        devprop_add_value(device, "AAPL,current-available", (UINT8*)&current_available_high, 2);
        devprop_add_value(device, "AAPL,current-extra",     (UINT8*)&current_extra_high, 2);
      } else {
        devprop_add_value(device, "AAPL,current-available", (UINT8*)&current_available, 2);
        devprop_add_value(device, "AAPL,current-extra",     (UINT8*)&current_extra, 2);
      }
      devprop_add_value(device, "AAPL,current-in-sleep",  (UINT8*)&current_in_sleep, 2);
      break;
    case PCI_IF_XHCI:
      devprop_add_value(device, "device_type", (UINT8*)"XHCI", 4);
      if (gSettings.Devices.USB.HighCurrent) {
        devprop_add_value(device, "AAPL,current-available", (UINT8*)&current_available_high, 2);
        devprop_add_value(device, "AAPL,current-extra",     (UINT8*)&current_extra_high, 2);
      } else {
        devprop_add_value(device, "AAPL,current-available", (UINT8*)&current_available, 2);
        devprop_add_value(device, "AAPL,current-extra",     (UINT8*)&current_extra, 2);
      }
      devprop_add_value(device, "AAPL,current-in-sleep",  (UINT8*)&current_in_sleep, 2);

      break;
    default:
      break;
  }
  return devprop_add_value(device, "built-in", (UINT8*)&builtin, 1);
}
