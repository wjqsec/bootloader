/*
 * refit/lib.c
 * General library functions
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile
#include <Efi.h>
#include "../include/OSTypes.h"
#include "lib.h"
#include "screen.h"
#include "../Platform/BasicIO.h"
#include "../Platform/BootLog.h"
#include "../Platform/guid.h"
#include "../Platform/APFS.h"
#include "../refit/lib.h"
#include "../Platform/Settings.h"
#include "../Settings/Self.h"
#include "../Settings/SelfOem.h"
#include "../Platform/Volumes.h"
#include "../libeg/XTheme.h"

#include "../include/OC.h"

#ifndef DEBUG_ALL
#define DEBUG_LIB 1
#else
#define DEBUG_LIB DEBUG_ALL
#endif

#if DEBUG_LIB == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_LIB, __VA_ARGS__)
#endif

// variables


//XStringW         ThemePath;
//XBool            gBootArgsChanged = false;
XBool              gThemeOptionsChanged = false;


//
// Unicode collation protocol interface
//
EFI_UNICODE_COLLATION_PROTOCOL *mUnicodeCollation = NULL;

// functions

static void UninitVolumes(void);

// S. Mtr
/* Function for parsing nodes from device path
 * IN : DevicePath, sizeof(DevicePath)
 * OUT: Size of cutted device path
 * Description:
 * Device path contains device nodes.
 * From UEFI specification device node struct looks like:
 *typedef struct {
 *  UINT8 Type;   ///< 0x01 Hardware Device Path.
 *                ///< 0x02 ACPI Device Path.
 *                ///< 0x03 Messaging Device Path.
 *                ///< 0x04 Media Device Path.
 *                ///< 0x05 BIOS Boot Specification Device Path.
 *                ///< 0x7F End of Hardware Device Path.
 *
 *  UINT8 SubType;///< Varies by Type
 *                ///< 0xFF End Entire Device Path, or
 *                ///< 0x01 End This Instance of a Device Path and start a new
 *                ///< Device Path.
 *
 *  UINT8 Length[2];  ///< Specific Device Path data. Type and Sub-Type define
 *                    ///< type of data. Size of data is included in Length.
 *
 * } EFI_DEVICE_PATH_PROTOCOL;
 */
UINTN
NodeParser  (UINT8 *DevPath, UINTN PathSize, UINT8 Type)
{
  UINTN i;
  for (i=0; i<PathSize+1;){
    if (DevPath[i] == Type)
    {
      //This type corresponds to Type
      //So.. save position and exit from loop
      PathSize = i;
      break;
    }
    //Jump to the next device node type
    i += (((UINT16)DevPath[i+3]<<8) | DevPath[i+2]);
  }
  return PathSize;
}

XBool MetaiMatch (
                    IN CONST CHAR16   *String,
                    IN CONST CHAR16   *Pattern
                    );



EFI_STATUS GetRootFromPath(IN EFI_DEVICE_PATH_PROTOCOL* DevicePath, OUT EFI_FILE **Root)
{
  EFI_STATUS  Status;
  EFI_HANDLE                NewHandle;
  EFI_DEVICE_PATH_PROTOCOL* TmpDevicePath;
  //  DBG("Try to duplicate DevicePath\n");
  TmpDevicePath = DuplicateDevicePath(DevicePath);
  //  DBG("TmpDevicePath found\n");
  NewHandle = NULL;
	Status = gBS->LocateDevicePath (&gEfiSimpleFileSystemProtocolGuid,
                                  &TmpDevicePath,
                                  &NewHandle);
  //   DBG("volume handle found =%X\n", NewHandle);
  CheckError(Status, L"while reopening volume handle");
  *Root = EfiLibOpenRoot(NewHandle);
  if (*Root == NULL) {
    //    DBG("volume Root Dir can't be reopened\n");
    return EFI_NOT_FOUND;
  }
  if (FileExists(*Root, L"mach_kernel")) {
    DBG("mach_kernel exists\n");
  } else {
    DBG("mach_kernel not exists\n");
  }
  
  return Status;
}
//
// self recognition stuff
//

EFI_STATUS InitRefitLib(IN EFI_HANDLE ImageHandle)
{
  self.initialize(ImageHandle);
  DBG("SelfDirPath = %ls\n", self.getCloverDirFullPath().wc_str());
  return EFI_SUCCESS;
}

void UninitRefitLib(void)
{
  // called before running external programs to close open file handles
  
  selfOem.closeHandle();
  self.closeHandle();
  
  closeDebugLog();
  UninitVolumes();
}

EFI_STATUS ReinitRefitLib(void)
{
  // called after reconnect drivers to re-open file handles
  
  self.reInitialize();
  selfOem.reInitialize();

  ReinitVolumes();

  return EFI_SUCCESS;
}



//
// firmware device path discovery
//

//looks like not used anywhere
static UINT8 LegacyLoaderMediaPathData[] = {
  0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
  0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
  0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
static EFI_DEVICE_PATH *LegacyLoaderMediaPath = (EFI_DEVICE_PATH *)LegacyLoaderMediaPathData;

EFI_STATUS ExtractLegacyLoaderPaths(EFI_DEVICE_PATH **PathList, UINTN MaxPaths, EFI_DEVICE_PATH **HardcodedPathList)
{
  EFI_STATUS          Status;
  UINTN               HandleCount = 0;
  UINTN               HandleIndex, HardcodedIndex;
  EFI_HANDLE          *Handles = NULL;
  EFI_HANDLE          Handle;
  UINTN               PathCount = 0;
  UINTN               PathIndex;
  EFI_LOADED_IMAGE    *LoadedImage;
  EFI_DEVICE_PATH     *DevicePath;
  XBool               Seen;
  
  MaxPaths--;  // leave space for the terminating NULL pointer
  
  // get all LoadedImage handles
  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL,
                                   &HandleCount, &Handles);
  if (CheckError(Status, L"while listing LoadedImage handles")) {
    if (HardcodedPathList) {
      for (HardcodedIndex = 0; HardcodedPathList[HardcodedIndex] && PathCount < MaxPaths; HardcodedIndex++)
        PathList[PathCount++] = HardcodedPathList[HardcodedIndex];
    }
    PathList[PathCount] = NULL;
    return Status;
  }
  for (HandleIndex = 0; HandleIndex < HandleCount && PathCount < MaxPaths; HandleIndex++) {
    Handle = Handles[HandleIndex];
    
    Status = gBS->HandleProtocol(Handle, &gEfiLoadedImageProtocolGuid, (void **) &LoadedImage);
    if (EFI_ERROR(Status))
      continue;  // This can only happen if the firmware scewed up, ignore it.
    
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiDevicePathProtocolGuid, (void **) &DevicePath);
    if (EFI_ERROR(Status))
      continue;  // This happens, ignore it.
    
    // Only grab memory range nodes
    if (DevicePathType(DevicePath) != HARDWARE_DEVICE_PATH || DevicePathSubType(DevicePath) != HW_MEMMAP_DP)
      continue;
    
    // Check if we have this device path in the list already
    // WARNING: This assumes the first node in the device path is unique!
    Seen = false;
    for (PathIndex = 0; PathIndex < PathCount; PathIndex++) {
      if (DevicePathNodeLength(DevicePath) != DevicePathNodeLength(PathList[PathIndex]))
        continue;
      if (CompareMem(DevicePath, PathList[PathIndex], DevicePathNodeLength(DevicePath)) == 0) {
        Seen = true;
        break;
      }
    }
    if (Seen)
      continue;
    
    PathList[PathCount++] = AppendDevicePath(DevicePath, LegacyLoaderMediaPath);
  }
  FreePool(Handles);
  
  if (HardcodedPathList) {
    for (HardcodedIndex = 0; HardcodedPathList[HardcodedIndex] && PathCount < MaxPaths; HardcodedIndex++)
      PathList[PathCount++] = HardcodedPathList[HardcodedIndex];
  }
  PathList[PathCount] = NULL;
  return (PathCount > 0)?EFI_SUCCESS:EFI_NOT_FOUND;
}

//
// volume functions
//

static void ScanVolumeBootcode(IN OUT REFIT_VOLUME *Volume, OUT XBool *Bootable)
{
  EFI_STATUS              Status;
  UINT8                  *SectorBuffer;
  UINTN                   i;
  //MBR_PARTITION_INFO   *MbrTable;
  //XBool                 MbrTableFound;
  UINTN                   BlockSize = 0;
  CHAR16                  volumeName[255];
  CHAR8                   tmp[64];
  UINT32                  VCrc32;
  //CHAR16               *kind = NULL;
  
  Volume->HasBootCode = false;
  Volume->LegacyOS.IconName.setEmpty();
  Volume->LegacyOS.Name.setEmpty();
  //  Volume->BootType = BOOTING_BY_MBR; //default value
  Volume->BootType = BOOTING_BY_EFI;
  *Bootable = false;
  
  if ((Volume->BlockIO == NULL) || (!Volume->BlockIO->Media->MediaPresent))
    return;
  ZeroMem((CHAR8*)&tmp[0], 64);
  BlockSize = Volume->BlockIO->Media->BlockSize;
  if (BlockSize > 2048)
    return;   // our buffer is too small... the bred of thieve of cable
  SectorBuffer = (__typeof__(SectorBuffer))AllocateAlignedPages(EFI_SIZE_TO_PAGES (2048), 16); //align to 16 byte?! Poher
  ZeroMem((CHAR8*)&SectorBuffer[0], 2048);
  // look at the boot sector (this is used for both hard disks and El Torito images!)
  Status = Volume->BlockIO->ReadBlocks(Volume->BlockIO, Volume->BlockIO->Media->MediaId,
                                       Volume->BlockIOOffset /*start lba*/,
                                       2048, SectorBuffer);
  if (!EFI_ERROR(Status) && (SectorBuffer[1] != 0)) {
    // calc crc checksum of first 2 sectors - it's used later for legacy boot BIOS drive num detection
    // note: possible future issues with AF 4K disks
    *Bootable = true;
    Volume->HasBootCode = true; //we assume that all CD are bootable
    /*      DBG("check SectorBuffer\n");
     for (i=0; i<32; i++) {
     DBG("%2hhX ", SectorBuffer[i]);
     }
     DBG("\n"); */
    VCrc32 = GetCrc32(SectorBuffer, 512 * 2);
    Volume->DriveCRC32 = VCrc32;
    //gBS->CalculateCrc32 (SectorBuffer, 2 * 512, &Volume->DriveCRC32);
    /*    switch (Volume->DiskKind ) {
     case DISK_KIND_OPTICAL:
     kind = L"DVD";
     break;
     case DISK_KIND_INTERNAL:
     kind = L"HDD";
     break;
     case DISK_KIND_EXTERNAL:
     kind = L"USB";
     break;
     default:
     break;
     }
     DBG("Volume kind=%ls CRC=0x%hhX\n", kind, VCrc32); */
    if (Volume->DiskKind == DISK_KIND_OPTICAL) { //CDROM
      CHAR8* p = (CHAR8*)&SectorBuffer[8];
      while (*p == 0x20) {
        p++;
      }
      for (i=0; i<30 && (*p >= 0x20) && (*p <= 'z'); i++, p++) {
        tmp[i] = *p;
      }
      tmp[i] = 0;
      while ((i>0) && (tmp[--i] == 0x20)) {}
      tmp[i+1] = 0;
			//	if (*p != 0) {
      AsciiStrToUnicodeStrS((CHAR8*)&tmp[0], volumeName, 255);
			//	}
      DBG("Detected name %ls\n", volumeName);
      Volume->VolName.takeValueFrom(volumeName);
      for (i=8; i<2000; i++) { //vendor search
        if (SectorBuffer[i] == 'A') {
          if (AsciiStrStr((CHAR8*)&SectorBuffer[i], "APPLE")) {
            //		StrCpy(Volume->VolName, volumeName);
            DBG("        Found AppleDVD\n");
            Volume->LegacyOS.Type = OSTYPE_OSX;
            Volume->BootType = BOOTING_BY_CD;
            Volume->LegacyOS.IconName = L"mac"_XSW;
            break;
          }
        } else if (SectorBuffer[i] == 'M') {
          if (AsciiStrStr((CHAR8*)&SectorBuffer[i], "MICROSOFT")) {
            //		StrCpy(Volume->VolName, volumeName);
            DBG("        Found Windows DVD\n");
            Volume->LegacyOS.Type = OSTYPE_WIN;
            Volume->BootType = BOOTING_BY_CD;
            Volume->LegacyOS.IconName = L"win"_XSW;
            break;
          }
          
        } else if (SectorBuffer[i] == 'L') {
          if (AsciiStrStr((CHAR8*)&SectorBuffer[i], "LINUX")) {
            //		Volume->DevicePath = DuplicateDevicePath(DevicePath);
            
            //		StrCpy(Volume->VolName, volumeName);
            DBG("        Found Linux DVD\n");
            Volume->LegacyOS.Type = OSTYPE_LIN;
            Volume->BootType = BOOTING_BY_CD;
            Volume->LegacyOS.IconName = L"linux"_XSW;
            break;
          }
        }
      }
      
    }
    //else HDD
    else { //HDD
      /*
       // apianti - does this detect every partition as legacy?
       if (*((UINT16 *)(SectorBuffer + 510)) == 0xaa55 && SectorBuffer[0] != 0) {
       *Bootable = true;
       Volume->HasBootCode = true;
       //    DBG("The volume has bootcode\n");
       Volume->LegacyOS.IconName = L"legacy";
       Volume->LegacyOS.Name = L"Legacy";
       Volume->LegacyOS.Type = OSTYPE_VAR;
       Volume->BootType = BOOTING_BY_PBR;
       }
       // */
      
      // detect specific boot codes
      if (CompareMem(SectorBuffer + 2, "LILO", 4) == 0 ||
          CompareMem(SectorBuffer + 6, "LILO", 4) == 0 ||
          CompareMem(SectorBuffer + 3, "SYSLINUX", 8) == 0 ||
          FindMem(SectorBuffer, 2048, "ISOLINUX", 8) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"linux"_XSW;
        Volume->LegacyOS.Name = L"Linux"_XSW;
        Volume->LegacyOS.Type = OSTYPE_LIN;
        Volume->BootType = BOOTING_BY_PBR;
        
      } else if (FindMem(SectorBuffer, 512, "Geom\0Hard Disk\0Read\0 Error", 26) >= 0) {   // GRUB
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"grub,linux"_XSW;
        Volume->LegacyOS.Name = L"Linux"_XSW;
        Volume->BootType = BOOTING_BY_PBR;
        /*
      } else if ((*((UINT32 *)(SectorBuffer)) == 0x4d0062e9 &&
                  *((UINT16 *)(SectorBuffer + 510)) == 0xaa55) ||
                 FindMem(SectorBuffer, 2048, "BOOT      ", 10) >= 0) { //reboot Clover
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"clover";
        Volume->LegacyOS.Name = L"Clover";
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        //        DBG("Detected Clover FAT32 bootcode\n");
     */   
        
      } else if ((*((UINT32 *)(SectorBuffer + 502)) == 0 &&
                  *((UINT32 *)(SectorBuffer + 506)) == 50000 &&
                  *((UINT16 *)(SectorBuffer + 510)) == 0xaa55) ||
                 FindMem(SectorBuffer, 2048, "Starting the BTX loader", 23) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"freebsd,linux"_XSW;
        Volume->LegacyOS.Name = L"FreeBSD"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        
        
      } else if (FindMem(SectorBuffer, 512, "!Loading", 8) >= 0 ||
                 FindMem(SectorBuffer, 2048, "/cdboot\0/CDBOOT\0", 16) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"openbsd,linux"_XSW;
        Volume->LegacyOS.Name = L"OpenBSD"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        
      } else if (FindMem(SectorBuffer, 512, "Not a bootxx image", 18) >= 0 ||
                 *((UINT32 *)(SectorBuffer + 1028)) == 0x7886b6d1) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"netbsd,linux"_XSW;
        Volume->LegacyOS.Name = L"NetBSD"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        
      } else if (FindMem(SectorBuffer, 2048, "NTLDR", 5) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"win"_XSW;
        Volume->LegacyOS.Name = L"Windows"_XSW;
        Volume->LegacyOS.Type = OSTYPE_WIN;
        Volume->BootType = BOOTING_BY_PBR;
        
        
      } else if (FindMem(SectorBuffer, 2048, "BOOTMGR", 7) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"vista,win"_XSW;
        Volume->LegacyOS.Name = L"Windows"_XSW;
        Volume->LegacyOS.Type = OSTYPE_WIN;
        Volume->BootType = BOOTING_BY_PBR;
        
      } else if (FindMem(SectorBuffer, 512, "CPUBOOT SYS", 11) >= 0 ||
                 FindMem(SectorBuffer, 512, "KERNEL  SYS", 11) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"freedos,win"_XSW;
        Volume->LegacyOS.Name = L"FreeDOS"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;

      } else if (FindMem(SectorBuffer, 512, "OS2LDR", 6) >= 0 ||
                 FindMem(SectorBuffer, 512, "OS2BOOT", 7) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"ecomstation"_XSW;
        Volume->LegacyOS.Name = L"eComStation"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        
      } else if (FindMem(SectorBuffer, 512, "Be Boot Loader", 14) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"beos"_XSW;
        Volume->LegacyOS.Name = L"BeOS"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        
      } else if (FindMem(SectorBuffer, 512, "yT Boot Loader", 14) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"zeta"_XSW;
        Volume->LegacyOS.Name = L"ZETA"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;
        
      } else if (FindMem(SectorBuffer, 512, "\x04" "beos\x06" "system\x05" "zbeos", 18) >= 0 ||
                 FindMem(SectorBuffer, 512, "haiku_loader", 12) >= 0) {
        Volume->HasBootCode = true;
        Volume->LegacyOS.IconName = L"haiku"_XSW;
        Volume->LegacyOS.Name = L"Haiku"_XSW;
        Volume->LegacyOS.Type = OSTYPE_VAR;
        Volume->BootType = BOOTING_BY_PBR;

      } 
    }
    
    // NOTE: If you add an operating system with a name that starts with 'W' or 'L', you
    //  need to fix AddLegacyEntry in main.c.
    
#if REFIT_DEBUG > 0
    DBG("        Result of bootcode detection: %ls %ls (%ls)\n",
        Volume->HasBootCode ? L"bootable" : L"non-bootable",
        Volume->LegacyOS.Name.notEmpty() ? Volume->LegacyOS.Name.wc_str() : L"unknown",
        Volume->LegacyOS.IconName.notEmpty() ? Volume->LegacyOS.IconName.wc_str() : L"legacy");
#endif

    if (FindMem(SectorBuffer, 512, "Non-system disk", 15) >= 0)   // dummy FAT boot sector
      Volume->HasBootCode = false;

#ifdef JIEF_DEBUG
////*Bootable = true;
//Volume->HasBootCode = true;
//Volume->LegacyOS.IconName = L"win"_XSW;
//Volume->LegacyOS.Name = L"Windows"_XSW;
//Volume->LegacyOS.Type = OSTYPE_WIN;
//Volume->BootType = BOOTING_BY_PBR;
#endif

    // check for MBR partition table
    /*
     // apianti - this is littered with bugs and probably not needed lol
     if (*((UINT16 *)(SectorBuffer + 510)) == 0xaa55) {
     MbrTableFound = false;
     MbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);
     for (i = 0; i < 4; i++)
     if (MbrTable[i].StartLBA && MbrTable[i].Size)
     MbrTableFound = true;
     for (i = 0; i < 4; i++)
     if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80)
     MbrTableFound = false;
     if (MbrTableFound) {
     Volume->MbrPartitionTable = (__typeof__(Volume->MbrPartitionTable))AllocatePool(4 * 16);
     CopyMem(Volume->MbrPartitionTable, MbrTable, 4 * 16);
     Volume->BootType = BOOTING_BY_MBR;
     }
     }
     // */
  }
//  gBS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer, 1);
//  FreeAlignedPages((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer, 1);
  FreeAlignedPages((void*)SectorBuffer, EFI_SIZE_TO_PAGES (2048));
}

//at start we have only Volume->DeviceHandle
static EFI_STATUS ScanVolume(IN OUT REFIT_VOLUME *Volume)
{
  EFI_STATUS              Status;
  HARDDRIVE_DEVICE_PATH   *HdPath     = NULL;
  EFI_HANDLE              WholeDiskHandle;
  UINTN                   PartialLength = 0;
  UINTN                   DevicePathSize;
  //  UINTN                   BufferSize = 255;
  EFI_FILE_SYSTEM_INFO    *FileSystemInfoPtr;
  EFI_FILE_INFO           *RootInfo = NULL;
  XBool                   Bootable;
  //  EFI_INPUT_KEY           Key;
  
  {
    // get device path
    EFI_DEVICE_PATH* DiskDevicePath = DevicePathFromHandle(Volume->DeviceHandle);
  //Volume->DevicePath = DuplicateDevicePath(DevicePathFromHandle(Volume->DeviceHandle));
    DevicePathSize = GetDevicePathSize (DiskDevicePath);
    Volume->DevicePath = (__typeof__(Volume->DevicePath))AllocateAlignedPages(EFI_SIZE_TO_PAGES(DevicePathSize), 64);
    CopyMem(Volume->DevicePath, DiskDevicePath, DevicePathSize);
    Volume->DevicePathString = FileDevicePathToXStringW(Volume->DevicePath);
  }
  
  if (Volume->DevicePath != NULL) {
    DBG(" %ls\n", FileDevicePathToXStringW(Volume->DevicePath).wc_str());
  }
  
  Volume->ApfsFileSystemUUID = APFSPartitionUUIDExtract(Volume->DevicePath); // NullXString8 if it's not an APFS volume
  Volume->DiskKind = DISK_KIND_INTERNAL;  // default
  
  // get block i/o
  Status = gBS->HandleProtocol(Volume->DeviceHandle, &gEfiBlockIoProtocolGuid, (void **) &(Volume->BlockIO));
  if (EFI_ERROR(Status)) {
    Volume->BlockIO = NULL;
 //   DBG("        Warning: Can't get BlockIO protocol.\n");
    //     WaitForSingleEvent (gST->ConIn->WaitForKey, 0);
    //     gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    
    return Status;
  }
  
  Bootable = false;
  if (Volume->BlockIO->Media->BlockSize == 2048){
//    DBG("        Found optical drive\n");
    Volume->DiskKind = DISK_KIND_OPTICAL;
    Volume->BlockIOOffset = 0x10; // offset already applied for FS but not for blockio
    ScanVolumeBootcode(Volume, &Bootable);
  } else {
    //        DBG("        Found HD drive\n");
    Volume->BlockIOOffset = 0;
    // scan for bootcode and MBR table
    ScanVolumeBootcode(Volume, &Bootable);
 //     DBG("        ScanVolumeBootcode success\n");
    // detect device type
    apd<EFI_DEVICE_PATH*> DevicePath = DuplicateDevicePath(Volume->DevicePath);
    EFI_DEVICE_PATH         *NextDevicePath = NextDevicePathNode(DevicePath);
    while (NextDevicePath != NULL && !IsDevicePathEndType(NextDevicePath))
    {
      if ((DevicePathType (NextDevicePath) == MESSAGING_DEVICE_PATH) &&
          ((DevicePathSubType (NextDevicePath) == MSG_SATA_DP) ||
           (DevicePathSubType (NextDevicePath) == MSG_NVME_NAMESPACE_DP) ||
           (DevicePathSubType (NextDevicePath) == MSG_ATAPI_DP))) {
  //                  DBG("        HDD volume\n");
            Volume->DiskKind = DISK_KIND_INTERNAL;
            break;
          }
      if (DevicePathType(NextDevicePath) == MESSAGING_DEVICE_PATH &&
          (DevicePathSubType(NextDevicePath) == MSG_USB_DP || DevicePathSubType(NextDevicePath) == MSG_USB_CLASS_DP)) {
   //     DBG("        USB volume\n");
        Volume->DiskKind = DISK_KIND_EXTERNAL;
        //     break;
      }
      // FIREWIRE Devices
      if (DevicePathType(NextDevicePath) == MESSAGING_DEVICE_PATH &&
          (DevicePathSubType(NextDevicePath) == MSG_1394_DP || DevicePathSubType(NextDevicePath) == MSG_FIBRECHANNEL_DP)) {
        //        DBG("        FireWire volume\n");
        Volume->DiskKind = DISK_KIND_FIREWIRE;
        break;
      }
      // CD-ROM Devices
      if (DevicePathType(NextDevicePath) == MEDIA_DEVICE_PATH &&
          DevicePathSubType(NextDevicePath) == MEDIA_CDROM_DP) {
        //        DBG("        CD-ROM volume\n");
        Volume->DiskKind = DISK_KIND_OPTICAL;    //it's impossible
        break;
      }
      // VENDOR Specific Path
      if (DevicePathType(NextDevicePath) == MEDIA_DEVICE_PATH &&
          DevicePathSubType(NextDevicePath) == MEDIA_VENDOR_DP) {
  //              DBG("        Vendor volume\n");
        if ( Volume->ApfsFileSystemUUID.isNull() ) {
          Volume->DiskKind = DISK_KIND_NODISK; // Jief, don't know why DISK_KIND_NODISK in that case. That prevents Recovery badge to appear. If it's not APFS, let's do it like it was before.
        }
        break;
      }
      // LEGACY CD-ROM
      if (DevicePathType(NextDevicePath) == BBS_DEVICE_PATH &&
          (DevicePathSubType(NextDevicePath) == BBS_BBS_DP || DevicePathSubType(NextDevicePath) == BBS_TYPE_CDROM)) {
        //        DBG("        Legacy CD-ROM volume\n");
        Volume->DiskKind = DISK_KIND_OPTICAL;
        break;
      }
      // LEGACY HARDDISK
      if (DevicePathType(NextDevicePath) == BBS_DEVICE_PATH &&
          (DevicePathSubType(NextDevicePath) == BBS_BBS_DP || DevicePathSubType(NextDevicePath) == BBS_TYPE_HARDDRIVE)) {
  //              DBG("        Legacy HDD volume\n");
        Volume->DiskKind = DISK_KIND_INTERNAL;
        break;
      }
      //one more we must take into account
      // subtype = MSG_NVME_NAMESPACE_DP
      // diskKind = NVME
      //#define MSG_NVME_NAMESPACE_DP     0x17
      
      NextDevicePath = NextDevicePathNode(NextDevicePath);
    }
    
    /*  what is the bread?
     // Bootable = true;
     
     if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH &&
     DevicePathSubType(DevicePath) == MEDIA_VENDOR_DP) {
     Volume->IsAppleLegacy = true;             // legacy BIOS device entry
     // TODO: also check for Boot Camp EFI_GUID
     //gEfiPartTypeSystemPartGuid
     Bootable = false;   // this handle's BlockIO is just an alias for the whole device
     DBG("AppleLegacy device\n");
     }
     */
  }
  
  {
    apd<EFI_DEVICE_PATH*> DevicePath = DuplicateDevicePath(Volume->DevicePath);
    EFI_DEVICE_PATH* walkDevicePath = DevicePath; //initial value
    //
    // find the partition device path node
    //
    while (walkDevicePath && !IsDevicePathEnd (walkDevicePath)) {
      if ((DevicePathType (walkDevicePath) == MEDIA_DEVICE_PATH) &&
          (DevicePathSubType (walkDevicePath) == MEDIA_HARDDRIVE_DP)) {
        HdPath = (HARDDRIVE_DEVICE_PATH *)walkDevicePath;
        //			break;
      }
      walkDevicePath = NextDevicePathNode (walkDevicePath);
    }
  //    DBG("DevicePath scanned\n");
    if (HdPath) {
      //      printf("Partition found %s\n", DevicePathToStr((EFI_DEVICE_PATH *)HdPath));
      
      PartialLength = (UINTN)((UINT8 *)HdPath - (UINT8 *)(DevicePath.get()));
      if (PartialLength > 0x1000) {
        PartialLength = sizeof(EFI_DEVICE_PATH); //something wrong here but I don't want to be freezed
        //       return EFI_SUCCESS;
      }
      apd<EFI_DEVICE_PATH*> DiskDevicePath = (EFI_DEVICE_PATH *)AllocatePool(PartialLength + sizeof(EFI_DEVICE_PATH));
      CopyMem(DiskDevicePath, Volume->DevicePath, PartialLength);
      CopyMem((UINT8 *)DiskDevicePath.get() + PartialLength, walkDevicePath, sizeof(EFI_DEVICE_PATH)); //EndDevicePath
    //        DBG("WholeDevicePath  %ls\n", DevicePathToStr(DiskDevicePath));
      EFI_DEVICE_PATH* RemainingDevicePath = DiskDevicePath;
      Status = gBS->LocateDevicePath(&gEfiDevicePathProtocolGuid, &RemainingDevicePath, &WholeDiskHandle);
      if (EFI_ERROR(Status)) {
        DBG("Can't find WholeDevicePath: %s\n", efiStrError(Status));
      } else {
        Volume->WholeDiskDeviceHandle = WholeDiskHandle;
        Volume->WholeDiskDevicePath = DuplicateDevicePath(RemainingDevicePath);
        // look at the BlockIO protocol
        Status = gBS->HandleProtocol(WholeDiskHandle, &gEfiBlockIoProtocolGuid, (void **) &Volume->WholeDiskBlockIO);
        if (!EFI_ERROR(Status)) {
          //          DBG("WholeDiskBlockIO %hhX BlockSize=%d\n", Volume->WholeDiskBlockIO, Volume->WholeDiskBlockIO->Media->BlockSize);
          // check the media block size
          if (Volume->WholeDiskBlockIO->Media->BlockSize == 2048)
            Volume->DiskKind = DISK_KIND_OPTICAL;
          
        } else {
          Volume->WholeDiskBlockIO = NULL;
        //  DBG("no WholeDiskBlockIO: %s\n", efiStrError(Status));
          
          //CheckError(Status, L"from HandleProtocol");
        }
      }
    }
  }
  /*  else {
   DBG("HD path is not found\n"); //master volume!
   }*/
  
  //  if (GlobalConfig.isFastBoot()) {
  //    return EFI_SUCCESS;
  //  }
  
  if (!Bootable) {
#if REFIT_DEBUG > 0
    if (Volume->HasBootCode){
      DBG("  Volume considered non-bootable, but boot code is present\n");
      //        WaitForSingleEvent (gST->ConIn->WaitForKey, 0);
      //        gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    }
#endif
    Volume->HasBootCode = false;
  }
  
  // open the root directory of the volume
  Volume->RootDir = EfiLibOpenRoot(Volume->DeviceHandle);
  //  DBG("Volume->RootDir OK\n");
  if (Volume->RootDir == NULL) {
    //Print(L"Error: Can't open volume.\n");
    // TODO: signal that we had an error
    //Slice - there is LegacyBoot volume
    //properties are set before
    //    DBG("LegacyBoot volume\n");
    
    if (HdPath) {
      Volume->VolName = SWPrintf("Legacy HD%d", HdPath->PartitionNumber);
    } else if (Volume->VolName.isEmpty()) {
      Volume->VolName = L"Whole Disc Boot"_XSW;
    }
    if (Volume->LegacyOS.IconName.isEmpty())
      Volume->LegacyOS.IconName = L"legacy"_XSW;
    
    return EFI_SUCCESS;
  }
  
  if ( Volume->ApfsFileSystemUUID.notNull() ) {
    apd<APPLE_APFS_CONTAINER_INFO*> ApfsContainerInfo;
    apd<APPLE_APFS_VOLUME_INFO*>    ApfsVolumeInfo;
    Status = InternalGetApfsSpecialFileInfo(Volume->RootDir, &ApfsVolumeInfo.get(), &ApfsContainerInfo.get());
    if ( !EFI_ERROR(Status) ) {
      //DBG("Status : %s, APFS role : %x\n", efiStrError(Status), ApfsVolumeInfo->Role);
      Volume->ApfsRole = ApfsVolumeInfo->Role;
      Volume->ApfsContainerUUID = ApfsContainerInfo->Uuid;
    }else{
      MsgLog("Status : %s, APFS role : %x\n", efiStrError(Status), ApfsVolumeInfo->Role);
    }
  }
  if ( Volume->ApfsFileSystemUUID.notNull() ) {
    DBG("          apfsFileSystemUUID=%s, ApfsContainerUUID=%s, ApfsRole=0x%x\n", Volume->ApfsFileSystemUUID.toXString8().c_str(), Volume->ApfsContainerUUID.toXString8().c_str(), Volume->ApfsRole);
  }


  if ( FileExists(Volume->RootDir, L"\\.VolumeLabel.txt") ) {
      EFI_FILE*     FileHandle;
      Status = Volume->RootDir->Open(Volume->RootDir, &FileHandle, L"\\.VolumeLabel.txt", EFI_FILE_MODE_READ, 0);
      if (!EFI_ERROR(Status)) {
          CHAR8 Buffer[32+1];
          UINTN BufferSize = sizeof(Buffer)-sizeof(CHAR8);
          SetMem(Buffer, BufferSize+sizeof(CHAR8), 0);
          Status = FileHandle->Read(FileHandle, &BufferSize, Buffer);
          FileHandle->Close(FileHandle);
          if (!EFI_ERROR(Status)) {
              // strip line endings
             while (BufferSize > 0 && (Buffer[BufferSize-1]=='\n' || Buffer[BufferSize-1]=='\r')) {
               Buffer[--BufferSize]='\0';
             }
            Volume->VolLabel = SWPrintf("%s", Buffer);
          }
      }
  }
  
  // get volume name
  if (Volume->VolName.isEmpty()) {
    FileSystemInfoPtr = EfiLibFileSystemInfo(Volume->RootDir);
    if (FileSystemInfoPtr) {
      //DBG("  Volume name from FileSystem: '%ls'\n", FileSystemInfoPtr->VolumeLabel);
      Volume->VolName.takeValueFrom(FileSystemInfoPtr->VolumeLabel);
      FreePool(FileSystemInfoPtr);
    }
  }
  if (Volume->VolName.isEmpty()) {
    Volume->VolName = EfiLibFileSystemVolumeLabelInfo(Volume->RootDir);
    //DBG("  Volume name from VolumeLabel: '%ls'\n", Volume->VolName.wc_str());
  }
  if (Volume->VolName.isEmpty()) {
    RootInfo = EfiLibFileInfo (Volume->RootDir);
    if (RootInfo) {
      //DBG("  Volume name from RootFile: '%ls'\n", RootInfo->FileName);
      Volume->VolName.takeValueFrom(RootInfo->FileName);
      FreePool(RootInfo);
    }
  }
  if ( Volume->VolName.isEmpty() || Volume->VolName.isEqual("\\") || Volume->VolName.isEqual(L"/") )
  {
    void *Instance;
    if (!EFI_ERROR(gBS->HandleProtocol(Volume->DeviceHandle, &gEfiPartTypeSystemPartGuid, &Instance))) {
      Volume->VolName = L"EFI"_XSW;
    }
  }
  if (Volume->VolName.isEmpty()) {
//    DBG("Create unknown name\n");
    //        WaitForSingleEvent (gST->ConIn->WaitForKey, 0);
    //        gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    
    if (HdPath) {
      
      Volume->VolName.SWPrintf( "Unknown HD%d", HdPath->PartitionNumber);
      // NOTE: this is normal for Apple's VenMedia device paths
    } else {
      Volume->VolName = L"Unknown HD"_XSW; //To be able to free it
    }
  }

  // Browse all folders under root that looks like an UUID
  if ( Volume->ApfsFileSystemUUID.notNull() )
  {

		REFIT_DIR_ITER  DirIter;
		EFI_FILE_INFO  *DirEntry = NULL;
		DirIterOpen(Volume->RootDir, L"\\", &DirIter);
		while (DirIterNext(&DirIter, 1, L"*", &DirEntry)) {
		  if (DirEntry->FileName[0] == '.') {
			  //DBG("Skip dot entries: %ls\n", DirEntry->FileName);
        continue;
		  }
		  if ( EFI_GUID::IsValidGuidString(LStringW(DirEntry->FileName)) ) {
        EFI_GUID* guid_ptr = new EFI_GUID;
        guid_ptr->takeValueFrom(DirEntry->FileName, wcslen(DirEntry->FileName));
        if ( guid_ptr->notNull() ) Volume->ApfsTargetUUIDArray.AddReference(guid_ptr, true);
        else delete guid_ptr;
		  }
		}
		DirIterClose(&DirIter);
  }

  DBG("          label : %ls\n", Volume->getVolLabelOrOSXVolumeNameOrVolName().wc_str());
  
  //Status = GetOSVersion(Volume); NOTE: Sothor - We will find icon names later once we have found boot.efi on the volume //here we set Volume->IconName (tiger,leo,snow,lion,cougar, etc)
  
  return EFI_SUCCESS;
}

static void ScanExtendedPartition(REFIT_VOLUME *WholeDiskVolume, MBR_PARTITION_INFO *MbrEntry)
{
  EFI_STATUS              Status;
  REFIT_VOLUME           *Volume;
  UINT32                  ExtBase, ExtCurrent, NextExtCurrent;
  UINTN                   i;
  UINTN                   LogicalPartitionIndex = 4;
  UINT8                  *SectorBuffer;
  XBool                   Bootable;
  MBR_PARTITION_INFO     *EMbrTable;
  
  ExtBase = MbrEntry->StartLBA;
  SectorBuffer = (__typeof__(SectorBuffer))AllocateAlignedPages (EFI_SIZE_TO_PAGES (512), WholeDiskVolume->BlockIO->Media->IoAlign);
  
  for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
    // read current EMBR
    Status = WholeDiskVolume->BlockIO->ReadBlocks(WholeDiskVolume->BlockIO,
                                                  WholeDiskVolume->BlockIO->Media->MediaId,
                                                  ExtCurrent, 512, SectorBuffer);
    if (EFI_ERROR(Status))
      break;
    if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55)
      break;
    EMbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);
    
    // scan logical partitions in this EMBR
    NextExtCurrent = 0;
    for (i = 0; i < 4; i++) {
      if ((EMbrTable[i].Flags != 0x00 && EMbrTable[i].Flags != 0x80) ||
          EMbrTable[i].StartLBA == 0 || EMbrTable[i].Size == 0)
        break;
      if (IS_EXTENDED_PART_TYPE(EMbrTable[i].Type)) {
        // set next ExtCurrent
        NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
        break;
      } else {
        
        // found a logical partition
        Volume = new REFIT_VOLUME;
        Volume->DiskKind = WholeDiskVolume->DiskKind;
        Volume->IsMbrPartition = true;
        Volume->MbrPartitionIndex = LogicalPartitionIndex++;
        Volume->VolName = SWPrintf("Partition %llu", Volume->MbrPartitionIndex + 1);
        Volume->BlockIO = WholeDiskVolume->BlockIO;
        Volume->BlockIOOffset = ExtCurrent + EMbrTable[i].StartLBA;
        Volume->WholeDiskBlockIO = WholeDiskVolume->BlockIO;
        Volume->WholeDiskDeviceHandle = WholeDiskVolume->DeviceHandle;
        
        Bootable = false;
        ScanVolumeBootcode(Volume, &Bootable);
        if (!Bootable)
          Volume->HasBootCode = false;
        
        Volumes.AddReference(Volume, false);
//        AddListElement((void ***) &Volumes, &VolumesCount, Volume);
      }
    }
  }
  gBS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer, 1);
}

void ScanVolumes(void)
{
  EFI_STATUS              Status;
  UINTN                   HandleCount = 0;
//  UINTN                   HandleIndex;
  EFI_HANDLE              *Handles = NULL;
  REFIT_VOLUME            *WholeDiskVolume;
  UINTN                   VolumeIndex, VolumeIndex2;
  MBR_PARTITION_INFO      *MbrTable;
  UINTN                   PartitionIndex;
  UINT8                   *SectorBuffer1, *SectorBuffer2;
  UINTN                   SectorSum, i;
  //  EFI_DEVICE_PATH_PROTOCOL  *VolumeDevicePath;
  //  EFI_GUID                *Guid; //for debug only
  //  EFI_INPUT_KEY Key;
  
  //    DBG("Scanning volumes...\n");
  DbgHeader("ScanVolumes");
  
  // get all BlockIo handles
  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &Handles);
  if (Status == EFI_NOT_FOUND)
    return;
	DBG("Found %llu volumes with blockIO\n", HandleCount);
  // first pass: collect information about all handles
  for (UINTN HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    
    REFIT_VOLUME* Volume = new REFIT_VOLUME;
    Volume->DeviceHandle = Handles[HandleIndex];
    if (Volume->DeviceHandle == self.getSelfDeviceHandle()) {
      SelfVolume = Volume;
    }
    
	  DBG("- [%02llu]: Volume:", HandleIndex);
    
    Volume->Hidden = false; // default to not hidden
    
    Status = ScanVolume(Volume);
#ifdef JIEF_DEBUG
  DBG("          kind=%d\n", Volume->DiskKind);
#endif

    if (!EFI_ERROR(Status)) {
      Volume->Index = HandleIndex;
      Volumes.AddReference(Volume, true);
      for (size_t HVi = 0; HVi < gSettings.GUI.HVHideStrings.size(); HVi++) {
        if ( Volume->DevicePathString.containsIC(gSettings.GUI.HVHideStrings[HVi]) ||
             Volume->VolName.containsIC(gSettings.GUI.HVHideStrings[HVi])
           ) {
          Volume->Hidden = true;
          DBG("        hiding this volume\n");
        }
      }
      
//      Guid = FindGPTPartitionGuidInDevicePath(Volume->DevicePath);
      if (Volume->LegacyOS.IconName.isEmpty()) {
        Volume->LegacyOS.IconName = L"legacy"_XSW;
      }
//      DBG("  Volume '%ls', LegacyOS '%ls', LegacyIcon(s) '%ls', EFI_GUID = %s\n",
//          Volume->VolName, Volume->LegacyOS.Name ? Volume->LegacyOS.Name : L"", Volume->LegacyOS.IconName, Guid.toXString8().c_str());
      if (SelfVolume == Volume) {
        DBG("        This is SelfVolume !!\n");
      }
      
    } else {
		DBG("        wrong volume Nr%llu?!\n", HandleIndex);
      FreePool(Volume);
    }
  }
  gBS->FreePool(Handles); // use gBS->FreePool instead of FreePool to avoid message from MemoryTracker
  //  DBG("Found %d volumes\n", VolumesCount);
  if (SelfVolume == NULL){
    DBG("        WARNING: SelfVolume not found"); //Slice - and what?
    SelfVolume = new REFIT_VOLUME;
    SelfVolume->DeviceHandle = self.getSelfDeviceHandle();
    SelfVolume->DevicePath = DuplicateDevicePath(&self.getSelfDevicePath());
    SelfVolume->RootDir = const_cast<EFI_FILE*>(&self.getSelfVolumeRootDir()); // TODO : SelfVolume->RootDir should be const ! we should duplicate ?
    SelfVolume->DiskKind = DISK_KIND_BOOTER;
    SelfVolume->VolName = L"Clover"_XSW;
    SelfVolume->LegacyOS.Type = OSTYPE_EFI;
    SelfVolume->HasBootCode = true;
    SelfVolume->BootType = BOOTING_BY_PBR;
    //   AddListElement((void ***) &Volumes, &VolumesCount, SelfVolume);
    //    DBG("SelfVolume Nr %d created\n", VolumesCount);
  }
  
  // second pass: relate partitions and whole disk devices
  for (VolumeIndex = 0; VolumeIndex < Volumes.size(); VolumeIndex++) {
    REFIT_VOLUME* Volume = &Volumes[VolumeIndex];
    
    // check MBR partition table for extended partitions
    if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
        Volume->BlockIO == Volume->WholeDiskBlockIO && Volume->BlockIOOffset == 0 &&
        Volume->MbrPartitionTable != NULL) {
      DBG("        Volume %llu has MBR\n", VolumeIndex);
      MbrTable = Volume->MbrPartitionTable;
      for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
        if (IS_EXTENDED_PART_TYPE(MbrTable[PartitionIndex].Type)) {
          ScanExtendedPartition(Volume, MbrTable + PartitionIndex);
        }
      }
    }
    
    // search for corresponding whole disk volume entry
    WholeDiskVolume = NULL;
    if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
        Volume->BlockIO != Volume->WholeDiskBlockIO) {
      for (VolumeIndex2 = 0; VolumeIndex2 < Volumes.size(); VolumeIndex2++) {
        if (Volumes[VolumeIndex2].BlockIO == Volume->WholeDiskBlockIO &&
            Volumes[VolumeIndex2].BlockIOOffset == 0)
          WholeDiskVolume = &Volumes[VolumeIndex2];
      }
    }
    if (WholeDiskVolume != NULL && WholeDiskVolume->MbrPartitionTable != NULL) {
      // check if this volume is one of the partitions in the table
      MbrTable = WholeDiskVolume->MbrPartitionTable;
      SectorBuffer1 = (__typeof__(SectorBuffer1))AllocateAlignedPages (EFI_SIZE_TO_PAGES (512), 16);
      SectorBuffer2 = (__typeof__(SectorBuffer2))AllocateAlignedPages (EFI_SIZE_TO_PAGES (512), 16);
      
      for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
        // check size
        if ((UINT64)(MbrTable[PartitionIndex].Size) != Volume->BlockIO->Media->LastBlock + 1)
          continue;
        
        // compare boot sector read through offset vs. directly
        Status = Volume->BlockIO->ReadBlocks(Volume->BlockIO, Volume->BlockIO->Media->MediaId,
                                             Volume->BlockIOOffset, 512, SectorBuffer1);
        if (EFI_ERROR(Status))
          break;
        Status = Volume->WholeDiskBlockIO->ReadBlocks(Volume->WholeDiskBlockIO, Volume->WholeDiskBlockIO->Media->MediaId,
                                                      MbrTable[PartitionIndex].StartLBA, 512, SectorBuffer2);
        if (EFI_ERROR(Status))
          break;
        if (CompareMem(SectorBuffer1, SectorBuffer2, 512) != 0)
          continue;
        SectorSum = 0;
        for (i = 0; i < 512; i++)
          SectorSum += SectorBuffer1[i];
        if (SectorSum < 1000)
          continue;
        
        // TODO: mark entry as non-bootable if it is an extended partition
        
        // now we're reasonably sure the association is correct...
        Volume->IsMbrPartition = true;
        Volume->MbrPartitionTable = MbrTable;
        Volume->MbrPartitionIndex = PartitionIndex;
        if (Volume->VolName.isEmpty())
          Volume->VolName = SWPrintf("Partition %llu", PartitionIndex + 1);
        break;
      }
      
      gBS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer1, 1);
      gBS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)SectorBuffer2, 1);
    }
    
  }
}

static void UninitVolumes(void)
{
  REFIT_VOLUME            *Volume;
  UINTN                   VolumeIndex;
  
  for (VolumeIndex = 0; VolumeIndex < Volumes.size(); VolumeIndex++) {
    Volume = &Volumes[VolumeIndex];
    
    if (Volume->RootDir != NULL) {
      Volume->RootDir->Close(Volume->RootDir);
      Volume->RootDir = NULL;
    }
    
    Volume->DeviceHandle = NULL;
    Volume->BlockIO = NULL;
    Volume->WholeDiskBlockIO = NULL;
    Volume->WholeDiskDeviceHandle = NULL;
    FreePool(Volume);
  }
  Volumes.setEmpty();
}

void ReinitVolumes(void)
{
  EFI_STATUS              Status;
  REFIT_VOLUME            *Volume;
  UINTN                   VolumeIndex;
  //UINTN                   VolumesFound = 0; // Jief not sure what this is, see my comment at the end of this function
  const EFI_DEVICE_PATH  *RemainingDevicePath;
  EFI_HANDLE              DeviceHandle, WholeDiskHandle;
  
  for (VolumeIndex = 0; VolumeIndex < Volumes.size(); VolumeIndex++) {
    Volume = &Volumes[VolumeIndex];
    if (!Volume) {
      continue;
    }
	  DBG("Volume %llu at reinit found:\n", VolumeIndex);
    DBG("Volume->DevicePath=%ls\n", FileDevicePathToXStringW(Volume->DevicePath).wc_str());
    //olumesFound++;
    if (Volume->DevicePath != NULL) {
      // get the handle for that path
      RemainingDevicePath = Volume->DevicePath;

      Status = gBS->LocateDevicePath(&gEfiBlockIoProtocolGuid, const_cast<EFI_DEVICE_PATH**>(&RemainingDevicePath), &DeviceHandle);
      
      if (!EFI_ERROR(Status)) {
        Volume->DeviceHandle = DeviceHandle;
        
        // get the root directory
        Volume->RootDir = EfiLibOpenRoot(Volume->DeviceHandle);
        
      }
      //else
      //  CheckError(Status, L"from LocateDevicePath");
    }
    
    if (Volume->WholeDiskDevicePath.notNull()) {
      // get the handle for that path
      RemainingDevicePath = DuplicateDevicePath(Volume->WholeDiskDevicePath);
      Status = gBS->LocateDevicePath(&gEfiBlockIoProtocolGuid, const_cast<EFI_DEVICE_PATH**>(&RemainingDevicePath), &WholeDiskHandle);
      
      if (!EFI_ERROR(Status)) {
        Volume->WholeDiskBlockIO = (__typeof__(Volume->WholeDiskBlockIO))WholeDiskHandle;
        // get the BlockIO protocol
        Status = gBS->HandleProtocol(WholeDiskHandle, &gEfiBlockIoProtocolGuid, (void **) &Volume->WholeDiskBlockIO);
        if (EFI_ERROR(Status)) {
          Volume->WholeDiskBlockIO = NULL;
          CheckError(Status, L"from HandleProtocol");
        }
      }
      //else
      //  CheckError(Status, L"from LocateDevicePath");
    }
  }
// Jief : I'm not sure to understand the next line. Why would we change the count when we didn't change the array.
// This code is not currently not used.
// Beware if you want to reuse this.
//  VolumesCount = VolumesFound;
}

REFIT_VOLUME *FindVolumeByName(IN CONST CHAR16 *VolName)
{
  REFIT_VOLUME            *Volume;
  UINTN                   VolumeIndex;
  
  if (!VolName) {
    return NULL;
  }
  
  for (VolumeIndex = 0; VolumeIndex < Volumes.size(); VolumeIndex++) {
    Volume = &Volumes[VolumeIndex];
    if (!Volume) {
      continue;
    }
    if (Volume->VolName.isEqual(VolName) == 0) {
      return Volume;
    }
  }
  
  return NULL;
}

//
// file and dir functions
//

XBool FileExists(IN CONST EFI_FILE *Root, IN CONST CHAR16 *RelativePath)
{
  EFI_STATUS  Status;
  EFI_FILE    *TestFile = NULL;
  
  Status = Root->Open(Root, &TestFile, RelativePath, EFI_FILE_MODE_READ, 0);
  if (Status == EFI_SUCCESS) {
    if (TestFile && TestFile->Close) {
      TestFile->Close(TestFile);
    }
    return true;
  }
  return false;
}

XBool FileExists(const EFI_FILE *Root, const XStringW& RelativePath)
{
	return FileExists(Root, RelativePath.wc_str());
}

XBool FileExists(const EFI_FILE& Root, const XStringW& RelativePath)
{
  return FileExists(&Root, RelativePath.wc_str());
}

EFI_DEVICE_PATH_PROTOCOL* FileDevicePath(IN EFI_HANDLE Device, IN CONST XStringW& FileName)
{
  return FileDevicePath(Device, FileName.wc_str());
}

XBool DeleteFile(const EFI_FILE *Root, IN CONST CHAR16 *RelativePath)
{
  EFI_STATUS  Status;
  EFI_FILE    *File;
  EFI_FILE_INFO   *FileInfo;
  
  //DBG("DeleteFile: %ls\n", RelativePath);
  // open file for read/write to see if it exists, need write for delete
  Status = Root->Open(Root, &File, RelativePath, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
  //DBG(" Open: %s\n", efiStrError(Status));
  if (Status == EFI_SUCCESS) {
    // exists - check if it is a file
    FileInfo = EfiLibFileInfo(File);
    if (FileInfo == NULL) {
      // error
      //DBG(" FileInfo is NULL\n");
      File->Close(File);
      return false;
    }
    //DBG(" FileInfo attr: %hhX\n", FileInfo->Attribute);
    if ((FileInfo->Attribute & EFI_FILE_DIRECTORY) == EFI_FILE_DIRECTORY) {
      // it's directory - return error
      //DBG(" File is DIR\n");
      FreePool(FileInfo);
      File->Close(File);
      return false;
    }
    FreePool(FileInfo);
    // it's a file - delete it
    //DBG(" File is file\n");
    Status = File->Delete(File);
    //DBG(" Delete: %s\n", efiStrError(Status));
    
    return Status == EFI_SUCCESS;
  }
  return false;
}

EFI_STATUS DirNextEntry(const EFI_FILE *Directory, IN OUT EFI_FILE_INFO **DirEntry, IN UINTN FilterMode)
{
  EFI_STATUS Status;
  void *Buffer;
  UINTN LastBufferSize, BufferSize;
  INTN IterCount;
  
  for (;;) {
    
    // free pointer from last call
    if (*DirEntry != NULL) {
      FreePool(*DirEntry);
      *DirEntry = NULL;
    }
    
    // read next directory entry
    LastBufferSize = BufferSize = 256;
    Buffer = (__typeof__(Buffer))AllocateZeroPool(BufferSize);
    for (IterCount = 0; ; IterCount++) {
      Status = Directory->Read(Directory, &BufferSize, Buffer);
      if (Status != EFI_BUFFER_TOO_SMALL || IterCount >= 4)
        break;
      if (BufferSize <= LastBufferSize) {
		  DBG("FS Driver requests bad buffer size %llu (was %llu), using %llu instead\n", BufferSize, LastBufferSize, LastBufferSize * 2);
        BufferSize = LastBufferSize * 2;
#if REFIT_DEBUG > 0
      } else {
		  DBG("Reallocating buffer from %llu to %llu\n", LastBufferSize, BufferSize);
#endif
      }
      Buffer = (__typeof__(Buffer))EfiReallocatePool(Buffer, LastBufferSize, BufferSize);
      LastBufferSize = BufferSize;
    }
    if (EFI_ERROR(Status)) {
      FreePool(Buffer);
      break;
    }
    
    // check for end of listing
    if (BufferSize == 0) {    // end of directory listing
      FreePool(Buffer);
      break;
    }
    
    // entry is ready to be returned
    *DirEntry = (EFI_FILE_INFO *)Buffer;
    if (*DirEntry) {
      // filter results
      if (FilterMode == 1) {   // only return directories
        if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY))
          break;
      } else if (FilterMode == 2) {   // only return files
        if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0)
          break;
      } else                   // no filter or unknown filter -> return everything
        break;
    }
  }
  return Status;
}

void DirIterOpen(const EFI_FILE *BaseDir, IN CONST CHAR16 *RelativePath OPTIONAL, OUT REFIT_DIR_ITER *DirIter)
{
  if (RelativePath == NULL) {
    DirIter->LastStatus = EFI_SUCCESS;
    DirIter->DirHandle = BaseDir;
    DirIter->CloseDirHandle = false;
  } else {
    DirIter->LastStatus = BaseDir->Open(BaseDir, const_cast<EFI_FILE**>(&(DirIter->DirHandle)), RelativePath, EFI_FILE_MODE_READ, 0);
    DirIter->CloseDirHandle = EFI_ERROR(DirIter->LastStatus) ? false : true;
  }
  DirIter->LastFileInfo = NULL;
}

XBool DirIterNext(IN OUT REFIT_DIR_ITER *DirIter, IN UINTN FilterMode, IN CONST CHAR16 *FilePattern OPTIONAL,
                    OUT EFI_FILE_INFO **DirEntry)
{
  if (DirIter->LastFileInfo != NULL) {
    FreePool(DirIter->LastFileInfo);
    DirIter->LastFileInfo = NULL;
  }
  
  if (EFI_ERROR(DirIter->LastStatus))
    return false;   // stop iteration
  
  for (;;) {
    DirIter->LastStatus = DirNextEntry(DirIter->DirHandle, &(DirIter->LastFileInfo), FilterMode);
    if (EFI_ERROR(DirIter->LastStatus))
      return false;
    if (DirIter->LastFileInfo == NULL)  // end of listing
      return false;
    if (FilePattern != NULL) {
      if ((DirIter->LastFileInfo->Attribute & EFI_FILE_DIRECTORY))
        break;
      if (MetaiMatch(DirIter->LastFileInfo->FileName, FilePattern))
        break;
      // else continue loop
    } else
      break;
  }
  
  *DirEntry = DirIter->LastFileInfo;
  return true;
}

EFI_STATUS DirIterClose(IN OUT REFIT_DIR_ITER *DirIter)
{
  if (DirIter->LastFileInfo != NULL) {
    FreePool(DirIter->LastFileInfo);
    DirIter->LastFileInfo = NULL;
  }
  if (DirIter->CloseDirHandle)
    DirIter->DirHandle->Close(DirIter->DirHandle);
  return DirIter->LastStatus;
}

//
// file name manipulation
//
XBool
MetaiMatch (
            IN CONST CHAR16   *String,
            IN CONST CHAR16   *Pattern
            )
{
	if (!mUnicodeCollation) {
		// quick fix for driver loading on UEFIs without UnicodeCollation
		//return false;
		return true; //this is wrong anyway
	}
	return mUnicodeCollation->MetaiMatch (mUnicodeCollation, String, Pattern) == TRUE;
}


EFI_STATUS
InitializeUnicodeCollationProtocol (void)
{
	EFI_STATUS  Status;
	
	if (mUnicodeCollation != NULL) {
		return EFI_SUCCESS;
	}
	
	//
	// BUGBUG: Proper implementation is to locate all Unicode Collation Protocol
	// instances first and then select one which support English language.
	// Current implementation just pick the first instance.
	//
	Status = gBS->LocateProtocol (
                                gEfiUnicodeCollation2ProtocolGuid,
                                NULL,
                                (void **) &mUnicodeCollation
                                );
  if (EFI_ERROR(Status)) {
    Status = gBS->LocateProtocol (
                                  gEfiUnicodeCollationProtocolGuid,
                                  NULL,
                                  (void **) &mUnicodeCollation
                                  );
    
  }
	return Status;
}



CONST CHAR16 * Basename(IN CONST CHAR16 *Path)
{
  CONST CHAR16  *FileName;
  UINTN   i;
  
  FileName = Path;
  
  if (Path != NULL) {
    for (i = StrLen(Path); i > 0; i--) {
      if (Path[i-1] == '\\' || Path[i-1] == '/') {
        FileName = Path + i;
        break;
      }
    }
  }
  
  return FileName;
}

void ReplaceExtension(IN OUT CHAR16 *Path, IN CHAR16 *Extension)
{
  INTN i;
  
  for (i = StrLen(Path); i >= 0; i--) {
    if (Path[i] == '.') {
      Path[i] = 0;
      break;
    }
    if (Path[i] == '\\' || Path[i] == '/')
      break;
  }
  StrCatS(Path, StrLen(Path)/sizeof(CHAR16)+1, Extension);
}

CHAR16 * egFindExtension(IN CHAR16 *FileName)
{
    INTN i;

    for (i = StrLen(FileName); i >= 0; i--) {
        if (FileName[i] == '.')
            return FileName + i + 1;
        if (FileName[i] == '/' || FileName[i] == '\\')
            break;
    }
    return FileName + StrLen(FileName);
}

//
// memory string search
//

INTN FindMem(IN CONST void *Buffer, IN UINTN BufferLength, IN CONST void *SearchString, IN UINTN SearchStringLength)
{
  CONST UINT8 *BufferPtr;
  UINTN Offset;
  
  BufferPtr = (CONST UINT8 *)Buffer;
  BufferLength -= SearchStringLength;
  for (Offset = 0; Offset < BufferLength; Offset++, BufferPtr++) {
    if (CompareMem(BufferPtr, SearchString, SearchStringLength) == 0)
      return (INTN)Offset;
  }
  
  return -1;
}

/**
 This function converts an input device structure to a Unicode string.

 @param DevPath                  A pointer to the device path structure.

 @return A new allocated Unicode string that represents the device path.

 **/
XStringW DevicePathToXStringW (
    const EFI_DEVICE_PATH_PROTOCOL     *DevPath
  )
{
  CHAR16* DevicePathStr = ConvertDevicePathToText (DevPath, true, true);
  XStringW returnValue;
  returnValue.stealValueFrom(DevicePathStr); // do not FreePool FilePath, it's now owned by returnValue
  return returnValue;

}

//
// Aptio UEFI returns File DevPath as 2 nodes (dir, file)
// and DevicePathToStr connects them with /, but we need '\\'
XStringW FileDevicePathToXStringW(const EFI_DEVICE_PATH_PROTOCOL *DevPath)
{
  CHAR16      *FilePath;
  CHAR16      *Char;
  CONST CHAR16      *Tail;
  
  FilePath = ConvertDevicePathToText(DevPath, true, true);
  // fix / into '\\'
  if (FilePath != NULL) {
    for (Char = FilePath; *Char != L'\0'; Char++) {
      if (*Char == L'/') {
        *Char = L'\\';
      }
    }
  }
  // "\\\\" into '\\'
  Char = (CHAR16*)StrStr(FilePath, L"\\\\"); // cast is ok because FilePath is not const, and we know that StrStr returns a pointer in FilePath. Will disappear when using a string object instead of CHAR16*
  while (Char != NULL) {
//    StrCpyS(Char, 4, Char + 1);  //can't overlap
    Tail = Char + 1;
    while (*Char != 0) {
      *(Char++) = *(Tail++);
    }
    Char = (CHAR16*)StrStr(FilePath, L"\\\\"); // cast is ok because FilePath is not const, and we know that StrStr returns a pointer in FilePath. Will disappear when using a string object instead of CHAR16*
  }
  XStringW returnValue;
  returnValue.stealValueFrom(FilePath); // do not FreePool FilePath, it's now owned by returnValue
  return returnValue;
}

XStringW FileDevicePathFileToXStringW(const EFI_DEVICE_PATH_PROTOCOL *DevPath)
{
  EFI_DEVICE_PATH_PROTOCOL *Node;
  
  if (DevPath == NULL) {
    return NullXStringW;
  }
  
  Node = (EFI_DEVICE_PATH_PROTOCOL *)DevPath;
  while (!IsDevicePathEnd(Node)) {
    if ((Node->Type == MEDIA_DEVICE_PATH) &&
        (Node->SubType == MEDIA_FILEPATH_DP)) {
      return FileDevicePathToXStringW(Node);
    }
    Node = NextDevicePathNode(Node);
  }
  return NullXStringW;
}

XBool DumpVariable(CHAR16* Name, const EFI_GUID& Guid, INTN DevicePathAt)
{
  UINTN                     dataSize            = 0;
  UINT8                     *data               = NULL;
  UINTN i;
  EFI_STATUS  Status;
  
  Status = gRT->GetVariable (Name, Guid, NULL, &dataSize, data);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    data = (__typeof__(data))AllocateZeroPool(dataSize);
    Status = gRT->GetVariable (Name, Guid, NULL, &dataSize, data);
    if (EFI_ERROR(Status)) {
		DBG("Can't get %ls, size=%llu\n", Name, dataSize);
      FreePool(data);
      data = NULL;
    } else {
		DBG("%ls var size=%llu\n", Name, dataSize);
      for (i = 0; i < dataSize; i++) {
        DBG("%02hhX ", data[i]);
      }
      DBG("\n");
      if (DevicePathAt >= 0) {
        DBG("%ls: %ls\n", Name, FileDevicePathToXStringW((EFI_DEVICE_PATH_PROTOCOL*)&data[DevicePathAt]).wc_str());
      }
    }
  }
  if (data) {
    FreePool(data);
    return true;
  }
  return false;
}

// EOF
