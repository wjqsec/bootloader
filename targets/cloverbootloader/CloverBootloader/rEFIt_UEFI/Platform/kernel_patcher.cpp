/*
 * Original idea of patching kernel by Evan Lojewsky, 2009
 *
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 * 2012 - 2020 Correction and improvements by Clover team
 */

//#include <IndustryStardard/MachO-loader.h>


#include <UefiLoader.h>

#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile
//#include "LoaderUefi.h"
#include "Nvram.h"
#include "FixBiosDsdt.h"
#include "cpu.h"
#include "kext_inject.h"

#include "kernel_patcher.h"
#include "MemoryOperation.h"
#include "../include/OSFlags.h"

//#include "sse3_patcher.h"
//#include "sse3_5_patcher.h"

#ifndef DEBUG_ALL
#define KERNEL_DEBUG 1
#else
#define KERNEL_DEBUG DEBUG_ALL
#endif


#if KERNEL_DEBUG == 2
#define DBG(...)    printf(__VA_ARGS__);
#elif KERNEL_DEBUG == 1
#define DBG(...)    DebugLog(KERNEL_DEBUG, __VA_ARGS__)
#else
#define DBG(...)
#endif

// runtime debug
//make it a member of LOADER_ENTRY class entry.DBG_RT(...)
//#define DBG_RT( ...)    if ((KernelAndKextPatches != NULL) && KernelAndKextPatches.KPDebug) { printf(__VA_ARGS__); }


//EFI_PHYSICAL_ADDRESS    KernelRelocBase = 0;
//BootArgs1   *bootArgs1 = NULL;
//BootArgs2   *bootArgs2 = NULL;
//CHAR8       *dtRoot = NULL;
//UINT32      *dtLength;
//UINT8       *KernelData = NULL;
//UINT32      KernelSlide = 0;
//XBool       isKernelcache = false;
//XBool       is64BitKernel = false;
//XBool       SSSE3;

//XBool       PatcherInited = false;
//XBool       gSNBEAICPUFixRequire = false; // SandyBridge-E AppleIntelCpuPowerManagement patch require or not
//XBool       gBDWEIOPCIFixRequire = false; // Broadwell-E IOPCIFamily fix require or not

/*
 * the driver OsxAptioFixDrv is old and mostly not used in favour of its successors.
 * anyway we will keep it for new investigations.
 */
void LOADER_ENTRY::SetKernelRelocBase()
{
//  EFI_STATUS      Status;
  UINTN           DataSize = sizeof(KernelRelocBase);

  KernelRelocBase = 0;
  // OsxAptioFixDrv will set this
  /*Status = */gRT->GetVariable(L"OsxAptioFixDrv-RelocBase", gEfiAppleBootGuid, NULL, &DataSize, &KernelRelocBase);
  DeleteNvramVariable(L"OsxAptioFixDrv-RelocBase", gEfiAppleBootGuid); // clean up the temporary variable
  // KernelRelocBase is now either read or 0
  return;
}

//Slice
// the purpose of the procedure is to find a table of symbols in the shifted kernel
EFI_STATUS LOADER_ENTRY::getVTable()
{
	DBG("kernel at 0x%llx\n", (UINTN)KernelData);
//  INT32 LinkAdr = FindBin(KernelData, 0x3000, (const UINT8 *)kLinkEditSegment, (UINT32)strlen(kLinkEditSegment));
//  if (LinkAdr == -1) {
//    DBG("no LinkEdit\n");
//    return EFI_NOT_FOUND;
//  }
//  const UINT8 vtable[] = {0x04, 00,00,00, 0x0F, 0x08, 00, 00};
//  const UINT8 vtableSur[] = {0x25, 00,00,00, 0x0F, 0x06, 00, 00};
  //25000000 0F060000 940BFF00 80FFFFFF
  //00FFFFFF FF0FFFFF 00000000 FFFFFFFF

 // INT32 Tabble  = FindBin(KernelData, 0x5000000, vtableSur, 8);
  INT32 NTabble = FindBin(KernelData, KERNEL_MAX_SIZE, (const UINT8 *)ctor_used, (UINT32)strlen(ctor_used));
  DBG("ctor_used found at 0x%x\n", NTabble);
  if (NTabble < 0) {
    return EFI_NOT_FOUND;
  }
  while (KernelData[NTabble] || KernelData[NTabble-1]) --NTabble;
  NTabble &= ~0x03; //align, may be 0x07?
//  NTabble -=4;
  DBG(" NTabble=%x\n", NTabble);
//	DBG("LinkAdr=%x NTabble=%x Tabble=%x\n",LinkAdr, NTabble, Tabble);
//  SEGMENT *LinkSeg = (SEGMENT*)&KernelData[LinkAdr];
//  AddrVtable = LinkSeg->AddrVtable;
//  SizeVtable = LinkSeg->SizeVtable;
//  NamesTable = LinkSeg->AddrNames;
  //TODO find an origin of the shift
  shift = NamesTable - NTabble;
//  DBG_RT("AddrVtable=%x Size=%x AddrNames=%x shift=%x\n", AddrVtable, SizeVtable, NamesTable, shift);
  NamesTable = NTabble;
  AddrVtable -= shift;
//  AddrVtable = Tabble;
  DBG("AddrVtable=%x Size=%x AddrNames=%x shift=%x\n", AddrVtable, SizeVtable, NamesTable, shift);
  SegVAddr = FindBin(KernelData+KernelOffset, 0x600, (const UINT8 *)kTextSegment, (UINT32)strlen(kTextSegment));
  SegVAddr += KernelOffset;
  DBG("SegVAddr=0x%x\n", SegVAddr);
  return EFI_SUCCESS;
}

UINT32 LOADER_ENTRY::searchSectionByNum(UINT8 * binary, UINT32 Num)
{
  UINT32  ncmds, cmdsize;
  UINT32  binaryIndex;
  UINT32  currsect = 0;
  UINT32  nsect;
  UINT32  textAddr = 0; // Init to avoid warning
  struct segment_command_64 *loadCommand;
  //  struct  symtab_command      *symCmd;
  if (!Num) {
    return 0;
  }
  
  ncmds = MACH_GET_NCMDS(binary);
  binaryIndex = sizeof(struct mach_header_64); //20
  DBG("segSize=0x%lx secsize=0x%lx\n", sizeof(struct segment_command_64), sizeof(struct section_64)); //48, 50
  
  for (UINTN cnt = 0; cnt < ncmds; cnt++) {
    loadCommand = (struct segment_command_64 *)(binary + binaryIndex); //20, 158
    cmdsize = loadCommand->cmdsize; //138, 278
    
    switch (loadCommand->cmd) {
    case LC_SEGMENT_64:
      nsect = loadCommand->nsects; //3, 7,
      if (currsect == 0) {
        textAddr = binaryIndex + sizeof(struct segment_command_64); //20+48=68
      }
      if (currsect + nsect >= Num - 1) { //3+7 >= 9
        UINT32 sectAddr = binaryIndex + sizeof(struct segment_command_64) + sizeof(struct section_64) * (Num - currsect - 1);
        //158+48+50*
        if (*(UINT32*)(binary + sectAddr) == 0x73625F5F) { //special case for __bss
          DBG("__bss will be used as __text\n");
          return textAddr;
        }
        return sectAddr;
      }
      currsect += nsect; //3
      break;
    default:
      break;
    }
    binaryIndex += cmdsize; //20+138=158,
  }
  return 0;
}

UINTN LOADER_ENTRY::searchProcInDriver(UINT8 * driver, UINT32 driverLen, const XString8& procedure)
{
  if (procedure.isEmpty()) {
    return 0;
  }
  /*
  INT32 LinkAdr = FindBin(driver, driverLen, (const UINT8 *)kLinkEditSegment, (UINT32)strlen(kLinkEditSegment));
  if (LinkAdr == -1) {
    return 0;
  }
  SEGMENT *LinkSeg = (SEGMENT*)&driver[LinkAdr];
//  INT32 lAddrVtable = LinkSeg->AddrVtable;
  INT32 lSizeVtable = LinkSeg->SizeVtable;
//  INT32 lNamesTable = LinkSeg->AddrNames;
  const char* Names = (const char*)(&driver[LinkSeg->AddrNames]);
  VTABLE * vArray = (VTABLE*)(&driver[LinkSeg->AddrVtable]);
   struct nlist_64 {
    union {
      uint32_t  n_strx; // index into the string table  //str_adr=stroff+n_strx
    } n_un;
    uint8_t n_type;        // type flag, see below
    uint8_t n_sect;        // section number or NO_SECT
    uint16_t n_desc;       // see <mach-o/stab.h>
    uint64_t n_value;      // value of this symbol (or stab offset)
   };

*/
  DBG("search procedure %s\n", procedure.c_str());
  struct nlist_64 * vArray = NULL;
  INT32 lSizeVtable = 0;
  const char* Names = NULL;
  struct  symtab_command *symCmd = NULL;
  UINT32 symCmdOffset = Get_Symtab(driver);
  DBG("symCmdOffset=0x%X\n", symCmdOffset); //0x418
  if (symCmdOffset != 0) {
    if ((((struct mach_header_64*)KernelData)->filetype) == MH_KERNEL_COLLECTION) {
      symCmd = (struct symtab_command *)&driver[symCmdOffset];
      vArray = (struct nlist_64*)(&KernelData[symCmd->symoff - shift]);
      lSizeVtable = symCmd->nsyms;
      Names = (const char*)(&KernelData[symCmd->stroff - shift]);
      DBG("driverKC: AddrVtable=0x%x SizeVtable=0x%x NamesTable=0x%x\n",
          symCmd->symoff - shift, lSizeVtable, symCmd->stroff - shift);
    } else {
      symCmd = (struct symtab_command *)&driver[symCmdOffset];
      vArray = (struct nlist_64*)(&driver[symCmd->symoff]);
      lSizeVtable = symCmd->nsyms;
      Names = (const char*)(&driver[symCmd->stroff]);
      DBG("driver: AddrVtable=0x%x SizeVtable=0x%x NamesTable=0x%x\n", symCmd->symoff, lSizeVtable, symCmd->stroff);
    }
  }

  if (!vArray || !lSizeVtable || !Names) {
    return 0;
  }
  INT32 i;
  UINT32 Offset = 0; // Init to avoid warning
  XBool found = false;
  for (i = 0; i < lSizeVtable; ++i) {
    Offset = vArray[i].n_un.n_strx;
    if (strstr(&Names[Offset], procedure.c_str())) {
      found = true;
      break;
    }
  }
  if (!found) {
    DBG("%s not found\n", procedure.c_str());
    return 0;
  }
  DBG("found section %d at pos=%d\n", vArray[i].n_sect, i);
  DBG("name offset=0x%x vtable_off=0x%lx\n", symCmd->stroff + Offset, symCmd->symoff + i * sizeof(struct nlist_64));
//  INT32 textAddr = searchSectionByNum(driver, 1);
  INT32 lSegVAddr = searchSectionByNum(driver, vArray[i].n_sect);
//  DBG("section begin:\n");
//  for (int j=0; j<20; ++j) {
//    DBG("%02X", driver[lSegVAddr+j]);
//  }
//  DBG("\n");

  /*
  switch (vArray[i].Seg) {
  case ID_SEG_DATA:
    lSegVAddr = FindBin(driver, 0x1600, (const UINT8 *)kDataSegment, (UINT32)strlen(kDataSegment));
    break;
  case ID_SEG_DATA_CONST:
  case ID_SEC_CONST:
    lSegVAddr = FindSection(driver, 0x1600, (const UINT8 *)kDataSegment, (const UINT8 *)kConstSection);
    break;
  case ID_SEG_TEXT_CONST:
    lSegVAddr = FindSection(driver, 0x1600, (const UINT8 *)kTextSegment, (const UINT8 *)kConstSection);
    break;
  case ID_SEG_DATA_COMMON:
    lSegVAddr = FindSection(driver, 0x1600, (const UINT8 *)kDataSegment, (const UINT8 *)kCommonSection);
    break;
  case ID_SEG_DATA_DATA2:
  case ID_SEG_DATA_DATA:
    lSegVAddr = FindSection(driver, 0x1600, (const UINT8 *)kDataSegment, (const UINT8 *)kDataSection);
    break;
  case ID_SEG_KLD:
  case ID_SEG_KLD2:
  case ID_SEG_KLD3:
    lSegVAddr = FindBin(driver, 0x2000, (const UINT8 *)kKldSegment, (UINT32)strlen(kKldSegment));
    break;
//  case ID_SEC_BSS:
//    lSegVAddr = FindSection(driver, 0x1600, (const UINT8 *)kDataSegment, (const UINT8 *)kBssSection);
//    break;
  case ID_SEC_BSS:  //it works this way
  case ID_SEG_TEXT:
  case ID_SEG_TEXT2:
    lSegVAddr = FindSection(driver, 0x600, (const UINT8 *)kTextSegment, (const UINT8 *)kPrelinkTextSection);
    break;
  case ID_SEG_HIB:
    lSegVAddr = FindBin(driver, 0x2000, (const UINT8 *)kHibSegment, (UINT32)strlen(kHibSegment));
    break;

//    lSegVAddr = FindBin(driver, 0x600, (const UINT8 *)kTextSegment, (UINT32)strlen(kTextSegment));
//    break;
  default:
    return vArray[i].ProcAddr;

  }
  if (lSegVAddr == 0) {
    lSegVAddr = 0x38;
  }
  SEGMENT *TextSeg = (SEGMENT*)&driver[lSegVAddr];
   */
  struct section_64 *TextSeg = (struct section_64*)&driver[lSegVAddr];
  UINT64 Absolut = TextSeg->addr;
  UINT64 FileOff = TextSeg->offset;
  DBG("Absolut=0x%llx Fileoff=0x%llx\n", Absolut, FileOff);
  UINTN procAddr = vArray[i].n_value - Absolut + FileOff;
//  UINT32 procAddr32 = (UINT32)(vArray[i].n_value); //it is not work
  if ((((struct mach_header_64*)KernelData)->filetype) == MH_KERNEL_COLLECTION) {
    procAddr -= shift;
  }
  DBG("procAddr=0x%llx\n", procAddr);
#if KERNEL_DEBUG
  if (Absolut != 0) {
    UINT8 *procVM;
    if ((((struct mach_header_64*)KernelData)->filetype) == MH_KERNEL_COLLECTION) {
      procVM = (UINT8*)&KernelData[procAddr];
    } else {
      procVM = (UINT8*)&driver[procAddr];
    }
    DBG("procedure begin:\n");
    for (int j=0; j<30; ++j) {
      DBG("%02X", procVM[j]);
    }
    DBG("\n");
  }
#endif
  return procAddr;
}

//static int N = 0;
//search a procedure by Name and return its offset in the kernel
UINTN LOADER_ENTRY::searchProc(const XString8& procedure)
{
  if (procedure.isEmpty()) {
    return 0;
  }
  DBG("search name in kernel: %s\n", procedure.c_str());
  const char* Names = (const char*)(&KernelData[NamesTable]);
  VTABLE * vArray = (VTABLE*)(&KernelData[AddrVtable]);
  //search for the name
//  gBS->Stall(9000000);
  size_t i;
  XBool found = false;
  for (i=0; i<SizeVtable; ++i) {
    size_t Offset = vArray[i].NameOffset;
    if (Offset == 0) break;
//    if (N < 10) {
//      DBG("Offset %lx Seg=%x\n", Offset, vArray[i].Seg);
//      DBG("Name to compare %s\n", &Names[Offset]);
//      N++;
//    }
//    DBG_RT("Offset %lx Seg=%x\n", Offset, vArray[i].Seg);
//    DBG_RT("Name to compare %s\n", &Names[Offset]);
//    Stall(3000000);
    if (AsciiStrStr(&Names[Offset], procedure.c_str()) != NULL) {
      found = true;
      break;
    }
  }
  if (!found) {
    return 0;
  }
//  INT32 SegVAddr;
//  DBG_RT(" segment %x \n", vArray[i].Seg);
  
/*
  switch (vArray[i].Seg) {
  case ID_SEG_TEXT:
    SegVAddr = FindBin(kernel, 0x600, (const UINT8 *)kTextSegment, (UINT32)strlen(kTextSegment));
    break;
  case ID_SEG_DATA:
    SegVAddr = FindBin(kernel, 0x1600, (const UINT8 *)kDataSegment, (UINT32)strlen(kDataSegment));
    break;
  case ID_SEG_DATA_CONST:
    SegVAddr = FindBin(kernel, 0x2000, (const UINT8 *)kDataConstSegment, (UINT32)strlen(kDataConstSegment));
    break;
  case ID_SEG_KLD:
  case ID_SEG_KLD2:
    SegVAddr = FindBin(kernel, 0x2000, (const UINT8 *)kKldSegment, (UINT32)strlen(kKldSegment));
    break;
  default:
    return 0;
  }
*/
  SEGMENT *TextSeg = (SEGMENT*)&KernelData[SegVAddr];
  UINT64 Absolut = TextSeg->SegAddress;  //KLD=C70000
  UINT64 FileOff = TextSeg->fileoff;     //950000
  UINT64 procAddr = vArray[i].ProcAddr - Absolut + FileOff;
/*
  UINT64 prevAddr;
  if (i == 0) {
    prevAddr = Absolut;
  } else {
    prevAddr = vArray[i-1].ProcAddr;
  }
  *procLen = vArray[i].ProcAddr - prevAddr; //never worked
 */
  DBG("kernel: procAddr=0x%llx\n", procAddr);
  return procAddr;
}

#if 0
//TimeWalker - extended and corrected for systems up to Yosemite
//TODO - Slice: no more needed
void LOADER_ENTRY::KernelPatcher_64()
{
  UINT8       *bytes = KernelData;
  UINT32      patchLocation=0, patchLocation1=0;
  UINT32      i;
  UINT32      switchaddr=0;
  UINT32      mask_family=0, mask_model=0;
  UINT32      cpuid_family_addr=0, cpuid_model_addr=0;
//  UINT64      os_version;

//  DBG_RT( "Looking for _cpuid_set_info _panic ...\n");

  // Determine location of _cpuid_set_info _panic call for reference
  // basically looking for info_p->cpuid_model = bitfield32(reg[eax],  7,  4);
  for (i=0; i<0x1000000; i++) {
    if (bytes[i+ 0] == 0xC7 && bytes[i+ 1] == 0x05 && bytes[i+ 5] == 0x00 &&
        bytes[i+ 6] == 0x07 && bytes[i+ 7] == 0x00 && bytes[i+ 8] == 0x00 && bytes[i+ 9] == 0x00 &&
        bytes[i-5] == 0xE8) { // matching 0xE8 for _panic call start
      patchLocation = i-5;
      break;
    }
  }

  if (!patchLocation) {
//    DBG_RT( "_cpuid_set_info Unsupported CPU _panic not found \n");
    return;
  }

//  os_version = AsciiOSVersionToUint64(macOSVersion);

  // make sure only kernels for OSX 10.6.0 to 10.7.3 are being patched by this approach
  if (macOSVersion >= AsciiOSVersionToUint64("10.6") && macOSVersion <= AsciiOSVersionToUint64("10.7.3")) {

//    DBG_RT( "will patch kernel for macOS 10.6.0 to 10.7.3\n");

    // remove tsc_init: unknown CPU family panic for kernels prior to 10.6.2 which still had Atom support
    if (macOSVersion < AsciiOSVersionToUint64("10.6.2")) {
      for (i=0; i<0x1000000; i++) {
        // find _tsc_init panic address by byte sequence 488d3df4632a00
        if (bytes[i] == 0x48 && bytes[i+1] == 0x8D && bytes[i+2] == 0x3D && bytes[i+3] == 0xF4 &&
            bytes[i+4] == 0x63 && bytes[i+5] == 0x2A && bytes[i+6] == 0x00) {
          patchLocation1 = i+9;
//          DBG_RT( "Found _tsc_init _panic address at 0x%08x\n",patchLocation1);
          break;
        }
      }

      // NOP _panic call
      if (patchLocation1) {
        bytes[patchLocation1 + 0] = 0x90;
        bytes[patchLocation1 + 1] = 0x90;
        bytes[patchLocation1 + 2] = 0x90;
        bytes[patchLocation1 + 3] = 0x90;
        bytes[patchLocation1 + 4] = 0x90;
      }
    }
    else { // assume patching logic for OSX 10.6.2 to 10.7.3

      /*
       Here is our case from CPUID switch statement, it sets CPUFAMILY_UNKNOWN
       C7051C2C5F0000000000   mov     dword [ds:0xffffff80008a22c0], 0x0 (example from 10.7)
       */
      switchaddr = patchLocation - 19;
//      DBG_RT( "switch statement patch location is 0x%08x\n", (switchaddr+6));

      if (bytes[switchaddr + 0] == 0xC7 && bytes[switchaddr + 1] == 0x05 &&
          bytes[switchaddr + 5] == 0x00 && bytes[switchaddr + 6] == 0x00 &&
          bytes[switchaddr + 7] == 0x00 && bytes[switchaddr + 8] == 0x00) {

        // Determine cpuid_family address from above mov operation
        cpuid_family_addr =
        bytes[switchaddr + 2] <<  0 |
        bytes[switchaddr + 3] <<  8 |
        bytes[switchaddr + 4] << 16 |
        bytes[switchaddr + 5] << 24;
        cpuid_family_addr = cpuid_family_addr + (switchaddr + 10);

        if (cpuid_family_addr) {

          // Determine cpuid_model address
          // for 10.6.2 kernels it's offset by 299 bytes from cpuid_family address
          if (macOSVersion ==  AsciiOSVersionToUint64("10.6.2")) {
            cpuid_model_addr = cpuid_family_addr - 0X12B;
          }
          // for 10.6.3 to 10.6.7 it's offset by 303 bytes
          else if (macOSVersion <= AsciiOSVersionToUint64("10.6.7")) {
            cpuid_model_addr = cpuid_family_addr - 0X12F;
          }
          // for 10.6.8 to 10.7.3 kernels - by 339 bytes
          else {
            cpuid_model_addr = cpuid_family_addr - 0X153;
          }

//          DBG_RT( "cpuid_family address: 0x%08x\n", cpuid_family_addr);
//          DBG_RT( "cpuid_model address: 0x%08x\n",  cpuid_model_addr);

          switchaddr += 6; // offset 6 bytes in mov operation to write a dword instead of zero

          // calculate mask for patching, cpuid_family mask not needed as we offset on a valid mask
          mask_model   = cpuid_model_addr - (switchaddr+14);
//          DBG_RT( "model mask 0x%08x\n", mask_model);

//          DBG_RT( "overriding cpuid_family and cpuid_model as CPUID_INTEL_PENRYN\n");
          bytes[switchaddr+0] = (CPUFAMILY_INTEL_PENRYN & 0x000000FF) >>  0;
          bytes[switchaddr+1] = (CPUFAMILY_INTEL_PENRYN & 0x0000FF00) >>  8;
          bytes[switchaddr+2] = (CPUFAMILY_INTEL_PENRYN & 0x00FF0000) >> 16;
          bytes[switchaddr+3] = (CPUFAMILY_INTEL_PENRYN & 0xFF000000) >> 24;

          // mov  dword [ds:0xffffff80008a216d], 0x2000117
          bytes[switchaddr+4] = 0xC7;
          bytes[switchaddr+5] = 0x05;
          bytes[switchaddr+6] = (UINT8)((mask_model & 0x000000FF) >> 0);
          bytes[switchaddr+7] = (UINT8)((mask_model & 0x0000FF00) >> 8);
          bytes[switchaddr+8] = (UINT8)((mask_model & 0x00FF0000) >> 16);
          bytes[switchaddr+9] = (UINT8)((mask_model & 0xFF000000) >> 24);
          bytes[switchaddr+10] = 0x17; // cpuid_model (Penryn)
          bytes[switchaddr+11] = 0x01; // cpuid_extmodel
          bytes[switchaddr+12] = 0x00; // cpuid_extfamily
          bytes[switchaddr+13] = 0x02; // cpuid_stepping

          // fill remainder with 4 NOPs
          for (i=14; i<18; i++) {
            bytes[switchaddr+i] = 0x90;
          }
        }
      }
      else {
//        DBG_RT( "Unable to determine cpuid_family address, patching aborted\n");
        return;
      }
    }

    // patch ssse3
    if (!SSSE3 && (AsciiStrnCmp(macOSVersion,"10.6",4)==0)) {
      Patcher_SSE3_6((void*)bytes);
    }
    if (!SSSE3 && (AsciiStrnCmp(macOSVersion,"10.7",4)==0)) {
      Patcher_SSE3_7();
    }
  }

  // all 10.7.4+ kernels share common CPUID switch statement logic,
  // it needs to be exploited in diff manner due to the lack of space
  else if (macOSVersion >= AsciiOSVersionToUint64("10.7.4")) {

    DBG_RT( "will patch kernel for macOS 10.7.4+\n");

    /*
     Here is our switchaddress location ... it should be case 20 from CPUID switch statement
     833D78945F0000  cmp        dword [ds:0xffffff80008a21d0], 0x0;
     7417            je         0xffffff80002a8d71
     */
    switchaddr = patchLocation-45;
    DBG_RT( "switch statement patch location is 0x%08x\n", switchaddr);

    if(bytes[switchaddr + 0] == 0x83 && bytes[switchaddr + 1] == 0x3D &&
       bytes[switchaddr + 5] == 0x00 && bytes[switchaddr + 6] == 0x00 &&
       bytes[switchaddr + 7] == 0x74) {

      // Determine cpuid_family address
      // 891D4F945F00    mov        dword [ds:0xffffff80008a21a0], ebx
      cpuid_family_addr =
      bytes[switchaddr - 4] <<  0 |
      bytes[switchaddr - 3] <<  8 |
      bytes[switchaddr - 2] << 16 |
      bytes[switchaddr - 1] << 24;
      cpuid_family_addr = cpuid_family_addr + switchaddr;

      if (cpuid_family_addr) {

        // Determine cpuid_model address
        // for 10.6.8+ kernels it's 339 bytes apart from cpuid_family address
        cpuid_model_addr = cpuid_family_addr - 0X153;

        DBG_RT( "cpuid_family address: 0x%08x\n", cpuid_family_addr);
        DBG_RT( "cpuid_model address: 0x%08x\n",  cpuid_model_addr);

        // Calculate masks for patching
        mask_family  = cpuid_family_addr - (switchaddr +15);
        mask_model   = cpuid_model_addr -  (switchaddr +25);
        DBG_RT( "\nfamily mask: 0x%08x \nmodel mask: 0x%08x\n", mask_family, mask_model);

        // retain original
        // test ebx, ebx
        bytes[switchaddr+0] = bytes[patchLocation-13];
        bytes[switchaddr+1] = bytes[patchLocation-12];
        // retain original, but move jump offset by 20 bytes forward
        // jne for above test
        bytes[switchaddr+2] = bytes[patchLocation-11];
        bytes[switchaddr+3] = bytes[patchLocation-10]+0x20;
        // mov ebx, 0x78ea4fbc
        bytes[switchaddr+4] = 0xBB;
        bytes[switchaddr+5] = (CPUFAMILY_INTEL_PENRYN & 0x000000FF) >>  0;
        bytes[switchaddr+6] = (CPUFAMILY_INTEL_PENRYN & 0x0000FF00) >>  8;
        bytes[switchaddr+7] = (CPUFAMILY_INTEL_PENRYN & 0x00FF0000) >> 16;
        bytes[switchaddr+8] = (CPUFAMILY_INTEL_PENRYN & 0xFF000000) >> 24;

        // mov dword, ebx
        bytes[switchaddr+9]  = 0x89;
        bytes[switchaddr+10] = 0x1D;
        // cpuid_cpufamily address 0xffffff80008a21a0
        bytes[switchaddr+11] = (UINT8)((mask_family & 0x000000FF) >> 0);
        bytes[switchaddr+12] = (UINT8)((mask_family & 0x0000FF00) >> 8);
        bytes[switchaddr+13] = (UINT8)((mask_family & 0x00FF0000) >> 16);
        bytes[switchaddr+14] = (UINT8)((mask_family & 0xFF000000) >> 24);

        // mov dword
        bytes[switchaddr+15] = 0xC7;
        bytes[switchaddr+16] = 0x05;
        // cpuid_model address 0xffffff80008b204d
        bytes[switchaddr+17] = (UINT8)((mask_model & 0x000000FF) >> 0);
        bytes[switchaddr+18] = (UINT8)((mask_model & 0x0000FF00) >> 8);
        bytes[switchaddr+19] = (UINT8)((mask_model & 0x00FF0000) >> 16);
        bytes[switchaddr+20] = (UINT8)((mask_model & 0xFF000000) >> 24);

        bytes[switchaddr+21] = 0x17; // cpuid_model
        bytes[switchaddr+22] = 0x01; // cpuid_extmodel
        bytes[switchaddr+23] = 0x00; // cpuid_extfamily
        bytes[switchaddr+24] = 0x02; // cpuid_stepping

        // fill remainder with 25 NOPs
        for (i=25; i<25+25; i++) {
          bytes[switchaddr+i] = 0x90;
        }
      }
    }
    else {
      DBG_RT( "Unable to determine cpuid_family address, patching aborted\n");
      return;
    }
  }
}

void LOADER_ENTRY::KernelPatcher_32()
{
  UINT8* bytes = KernelData;
  UINT32 patchLocation=0, patchLocation1=0;
  UINT32 i;
  UINT32 jumpaddr;

  DBG("Found _cpuid_set_info _panic Start\n");
  // _cpuid_set_info _panic address
  for (i=0; i<0x1000000; i++) {
    if (bytes[i] == 0xC7 && bytes[i+1] == 0x05 && bytes[i+6] == 0x07 && bytes[i+7] == 0x00 &&
        bytes[i+8] == 0x00 && bytes[i+9] == 0x00 && bytes[i+10] == 0xC7 && bytes[i+11] == 0x05 &&
        bytes[i-5] == 0xE8) {
      patchLocation = i-5;
      DBG("Found _cpuid_set_info _panic address at 0x%08X\n",patchLocation);
      break;
    }
  }

  if (!patchLocation) {
    DBG("Can't find _cpuid_set_info _panic address, patch kernel abort.\n"/*,i*/);
    return;
  }

  // this for 10.6.0 and 10.6.1 kernel and remove tsc.c unknow cpufamily panic
  //  c70424540e5900
  // find _tsc_init panic address
  for (i=0; i<0x1000000; i++) {
    // _cpuid_set_info _panic address
    if (bytes[i] == 0xC7 && bytes[i+1] == 0x04 && bytes[i+2] == 0x24 &&
        bytes[i+3] == 0x54 && bytes[i+4] == 0x0E && bytes[i+5] == 0x59 &&
        bytes[i+6] == 0x00) {
      patchLocation1 = i+7;
      DBG("Found _tsc_init _panic address at 0x%08X\n",patchLocation1);
      break;
    }
  }

  // found _tsc_init panic addres and patch it
  if (patchLocation1) {
    bytes[patchLocation1 + 0] = 0x90;
    bytes[patchLocation1 + 1] = 0x90;
    bytes[patchLocation1 + 2] = 0x90;
    bytes[patchLocation1 + 3] = 0x90;
    bytes[patchLocation1 + 4] = 0x90;
  }
  // end tsc.c panic

  //first move panic code total 5 bytes, if patch cpuid fail still can boot with kernel
  bytes[patchLocation + 0] = 0x90;
  bytes[patchLocation + 1] = 0x90;
  bytes[patchLocation + 2] = 0x90;
  bytes[patchLocation + 3] = 0x90;
  bytes[patchLocation + 4] = 0x90;

  jumpaddr = patchLocation;

  for (i=0;i<500;i++) {
    if (bytes[jumpaddr-i-3] == 0x85 && bytes[jumpaddr-i-2] == 0xC0 &&
        bytes[jumpaddr-i-1] == 0x75 ) {
      jumpaddr -= i;
      bytes[jumpaddr-1] = 0x77;
      if(bytes[patchLocation - 17] == 0xC7)
        bytes[jumpaddr] -=10;

      break;
    }
  }

  if (jumpaddr == patchLocation) {
    DBG("Can't Found jumpaddr address.\n");
    return;  //can't find jump location
  }
  // patch info_p->cpufamily to CPUFAMILY_INTEL_MEROM

  if (bytes[patchLocation - 17] == 0xC7) {
    bytes[patchLocation - 11] = (CPUFAMILY_INTEL_MEROM & 0x000000FF) >>  0;
    bytes[patchLocation - 10] = (CPUFAMILY_INTEL_MEROM & 0x0000FF00) >>  8;
    bytes[patchLocation -  9] = (CPUFAMILY_INTEL_MEROM & 0x00FF0000) >> 16;
    bytes[patchLocation -  8] = (CPUFAMILY_INTEL_MEROM & 0xFF000000) >> 24;
  }

  //patch info->cpuid_cpufamily
  bytes[patchLocation -  7] = 0xC7;
  bytes[patchLocation -  6] = 0x05;
  bytes[patchLocation -  5] = bytes[jumpaddr + 3];
  bytes[patchLocation -  4] = bytes[jumpaddr + 4];
  bytes[patchLocation -  3] = bytes[jumpaddr + 5];
  bytes[patchLocation -  2] = bytes[jumpaddr + 6];

  bytes[patchLocation -  1] = CPUIDFAMILY_DEFAULT; //cpuid_family  need alway set 0x06
  bytes[patchLocation +  0] = CPU_MODEL_MEROM;     //cpuid_model set CPU_MODEL_MEROM
  bytes[patchLocation +  1] = 0x01;                //cpuid_extmodel alway set 0x01
  bytes[patchLocation +  2] = 0x00;                //cpuid_extfamily alway set 0x00
  bytes[patchLocation +  3] = 0x90;
  bytes[patchLocation +  4] = 0x90;

  if (macOSVersion) {
    if (AsciiStrnCmp(macOSVersion,"10.7",4)==0) return;

    if (!SSSE3 && (AsciiStrnCmp(macOSVersion,"10.6",4)==0)) {
      Patcher_SSE3_6((void*)bytes);
    }
    if (!SSSE3 && (AsciiStrnCmp(macOSVersion,"10.5",4)==0)) {
      Patcher_SSE3_5((void*)bytes);
    }
  }
}
#endif
//Slice - FakeCPUID substitution, (c)2014
// _cpuid_set_info
//TODO remake to patterns
//procedure location
const UINT8 StrCpuid1_tigLeo[]  = {0xb9, 0x01, 0x00, 0x00, 0x00, 0x89, 0xc8, 0x0f, 0xa2};
const UINT8 StrCpuid1_snowLeo[] = {0xb8, 0x01, 0x00, 0x00, 0x00, 0x31, 0xdb, 0x89, 0xd9, 0x89, 0xda, 0x0f, 0xa2};
const UINT8 StrMsr8b[]          = {0xb9, 0x8b, 0x00, 0x00, 0x00, 0x0f, 0x32};

// Tiger/Leopard/Snow Leopard
/*
 This patch searches
  and eax, 0xf0   ||    and eax, 0x0f0000
  shr eax, 0x04   ||    shr eax, 0x10
 and replaces to
  mov eax, FakeModel  | mov eax, FakeExt
 */
const UINT8 TigLeoSLSearchModel[]  = {0x25, 0xf0, 0x00, 0x00, 0x00, 0xc1, 0xe8, 0x04};
const UINT8 TigLeoSLSearchExt[]    = {0x25, 0x00, 0x00, 0x0f, 0x00, 0xc1, 0xe8, 0x10};
const UINT8 TigLeoSLReplaceModel[] = {0xb8, 0x07, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90};

// Lion
/*
 This patch searches
  mov ecx, eax
  shr ecx, 0x04   ||  shr ecx, 0x10
 and replaces to
  mov ecx, FakeModel  || mov ecx, FakeExt
 */
const UINT8 LionSearchModel[]  = {0x89, 0xc1, 0xc1, 0xe9, 0x04};
const UINT8 LionSearchExt[]    = {0x89, 0xc1, 0xc1, 0xe9, 0x10};
const UINT8 LionReplaceModel[] = {0xb9, 0x07, 0x00, 0x00, 0x00};

// Mountain Lion/Mavericks
/*
 This patch searches
  mov bl, al     ||   shr eax, 0x10
  shr bl, 0x04   ||   and al,0x0f
 and replaces to
  mov ebx, FakeModel || mov eax, FakeExt
 */
const UINT8 MLMavSearchModel[]   = {0x88, 0xc3, 0xc0, 0xeb, 0x04};
const UINT8 MLMavSearchExt[]     = {0xc1, 0xe8, 0x10, 0x24, 0x0f};
const UINT8 MLMavReplaceModel[]  = {0xbb, 0x0a, 0x00, 0x00, 0x00};
const UINT8 MLMavReplaceExt[]    = {0xb8, 0x02, 0x00, 0x00, 0x00};

// Yosemite/El Capitan/Sierra
/*
 This patch searches
  mov cl, al     ||   mov ecx, eax
  shr cl, 0x04   ||   shr ecx, 0x10
 and replaces to
  mov ecx, FakeModel || mov ecx, FakeExt
 */
const UINT8 YosECSieSearchModel[]   = {0x88, 0xc1, 0xc0, 0xe9, 0x04};
const UINT8 YosECSieSearchExt[]     = {0x89, 0xc1, 0xc1, 0xe9, 0x10};
// Need to use LionReplaceModel

// High Sierra/Mojave @2c4baa {89 c1 c0 e9 04}
/*
 This patch searches
  mov ecx, ecx   ||   mov ecx, eax
  shr cl, 0x04   ||   shr ecx, 0x10
 and replaces to
  mov ecx, FakeModel || mov ecx, FakeExt
 */
const UINT8 HSieMojSearchModel[]   = {0x89, 0xc1, 0xc0, 0xe9, 0x04};
// Need to use YosECSieSearchExt, LionReplaceModel

// Catalina
/*
 This patch searches
  mov eax, r12   ||   mov eax, r12
  shr al, 0x4    ||   shr eax, 0x10
 and replaces to
  mov eax, FakeModel || mov eax, FakeExt
  nop                || nop
*/
const UINT8 CataSearchModel[]      = {0x44, 0x89, 0xE0, 0xC0, 0xE8, 0x04};
const UINT8 CataSearchExt[]        = {0x44, 0x89, 0xE0, 0xC1, 0xE8, 0x10};
const UINT8 CataReplaceMovEax[]    = {0xB8, 0x00, 0x00, 0x00, 0x00, 0x90}; // mov eax, val || nop


XBool LOADER_ENTRY::PatchCPUID(const UINT8* Location, INT32 LenLoc,
                   const UINT8* Search4, const UINT8* Search10, const UINT8* ReplaceModel,
                   const UINT8* ReplaceExt, INT32 Len)
{
  INT32 patchLocation=0, patchLocation1=0;
  INT32 Adr = 0, Num;
  XBool Patched = false;
  UINT8 FakeModel = (KernelAndKextPatches.FakeCPUID >> 4) & 0x0f;
  UINT8 FakeExt = (KernelAndKextPatches.FakeCPUID >> 0x10) & 0x0f;
  for (Num = 0; Num < 2; Num++) {
    Adr = FindBin(&KernelData[Adr], 0x800000 - Adr, Location, (UINT32)LenLoc);
    if (Adr < 0) {
      break;
    }
    DBG_RT( "found location at %x\n", Adr);
    patchLocation = FindBin(&KernelData[Adr], 0x100, Search4, (UINT32)Len);
    if (patchLocation > 0 && patchLocation < 70) {
      //found
      DBG_RT( "found Model location at %x\n", Adr + patchLocation);
      CopyMem(&KernelData[Adr + patchLocation], ReplaceModel, Len);
      KernelData[Adr + patchLocation + 1] = FakeModel;
      patchLocation1 = FindBin(&KernelData[Adr], 0x100, Search10, (UINT32)Len);
      if (patchLocation1 > 0 && patchLocation1 < 100) {
        DBG_RT( "found ExtModel location at %x\n", Adr + patchLocation1);
        CopyMem(&KernelData[Adr + patchLocation1], ReplaceExt, Len);
        KernelData[Adr + patchLocation1 + 1] = FakeExt;
      }
      Patched = true;
    }
  }
  return Patched;
}

void LOADER_ENTRY::KernelCPUIDPatch()
{
// Tiger/Leopard patterns
  DBG_RT( "CPUID: try Tiger/Leopard patch...\n");
  if (PatchCPUID(&StrCpuid1_tigLeo[0], sizeof(StrCpuid1_tigLeo), &TigLeoSLSearchModel[0],
                 &TigLeoSLSearchExt[0], &TigLeoSLReplaceModel[0], &TigLeoSLReplaceModel[0],
                 sizeof(TigLeoSLSearchModel))) {
    DBG_RT( "...done!\n");
    return;
  }
// Snow Leopard patterns
  DBG_RT( "CPUID: try Snow Leopard patch...\n");
  if (PatchCPUID(&StrCpuid1_snowLeo[0], sizeof(StrCpuid1_snowLeo), &TigLeoSLSearchModel[0],
                 &TigLeoSLSearchExt[0], &TigLeoSLReplaceModel[0], &TigLeoSLReplaceModel[0],
                 sizeof(TigLeoSLSearchModel))) {
    DBG_RT( "...done!\n");
    return;
  }
// Lion patterns
  DBG_RT( "CPUID: try Lion patch...\n");
  if (PatchCPUID(&StrMsr8b[0], sizeof(StrMsr8b), &LionSearchModel[0],
                 &LionSearchExt[0], &LionReplaceModel[0], &LionReplaceModel[0],
                 sizeof(LionSearchModel))) {
    DBG_RT( "...done!\n");
    return;
  }
// Mountain Lion/Mavericks patterns
  DBG_RT( "CPUID: try Mountain Lion/Mavericks patch...\n");
  if (PatchCPUID(&StrMsr8b[0], sizeof(StrMsr8b), &MLMavSearchModel[0],
                 &MLMavSearchExt[0], &MLMavReplaceModel[0], &MLMavReplaceExt[0],
                 sizeof(MLMavSearchModel))) {
    DBG_RT( "...done!\n");
    return;
  }
// Yosemite/El Capitan/Sierra patterns
  DBG_RT( "CPUID: try Yosemite/El Capitan/Sierra patch...\n");
  if (PatchCPUID(&StrMsr8b[0], sizeof(StrMsr8b), &YosECSieSearchModel[0],
                 &YosECSieSearchExt[0], &LionReplaceModel[0], &LionReplaceModel[0],
                 sizeof(YosECSieSearchModel))) {
    DBG_RT( "...done!\n");
    return;
  }
// High Sierra/Mojave patterns
// Sherlocks: 10.13/10.14
  DBG_RT( "CPUID: try High Sierra/Mojave patch...\n");
  if (PatchCPUID(&StrMsr8b[0], sizeof(StrMsr8b), &HSieMojSearchModel[0],
                 &YosECSieSearchExt[0], &LionReplaceModel[0], &LionReplaceModel[0],
                 sizeof(HSieMojSearchModel))) {
    DBG_RT( "...done!\n");
    return;
  }
// Catalina patterns
// PMheart: 10.15.DP1
  DBG_RT( "CPUID: try Catalina patch...\n");
  if (PatchCPUID(&StrMsr8b[0], sizeof(StrMsr8b), &CataSearchModel[0],
                 &CataSearchExt[0], &CataReplaceMovEax[0], &CataReplaceMovEax[0],
                 sizeof(CataSearchModel))) {
    DBG_RT( "...done!\n");
    return;
  }
}

#define NEW_PM 1

XBool LOADER_ENTRY::KernelPatchPm()
{
  DBG_RT("Patching kernel power management...\n");
#if NEW_PM

  //Slice
  //1. procedure xcpm_idle
  // wrmsr 0xe2 twice
  // B9E2000000 0F30 replace to eb05
//  UINTN procLen = 0;
  UINTN procLocation = searchProc("xcpm_idle"_XS8);
  const UINT8 findJmp[]  = {0xB9, 0xE2, 0x00, 0x00, 0x00, 0x0F, 0x30};
  const UINT8 patchJmp[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
  DBG_RT("==> xcpm_idle at %llx\n", procLocation);
  INTN Num = SearchAndReplace(&KernelData[procLocation], 0x400, findJmp, sizeof(findJmp), patchJmp, 0);
  DBG_RT("==> found %lld patterns\n", Num);
  //2. procedure xcpm_init
  // indirect call to _xcpm_core_scope_msrs and to _xcpm_SMT_scope_msrs
  //  488D3DDA317600                  lea        rdi, qword [ds:_xcpm_core_scope_msrs]
  //  BE0B000000                      mov        esi, 0xb => replace to eb0a
  //  31D2                            xor        edx, edx
  //  E87EFCFFFF                      call       sub_ffffff80004fa610 => check e8?
  // there are other occurence of _xcpm_core_scope_msrs so check 488D3D or E8 at .+7
  // or restrict len = 0x200
  procLocation = searchProc("xcpm_init"_XS8);
  UINTN symbol1 = searchProc("xcpm_core_scope_msrs"_XS8);
  UINTN patchLocation1 = FindRelative32(KernelData, procLocation, 0x200, symbol1);
  if (patchLocation1 != 0) {
    DBG_RT("=> xcpm_core_scope_msrs found at %llx\n", patchLocation1);
    if (KernelData[patchLocation1 + 7] == 0xE8) {
      DBG_RT("=> patch applied\n");
//      for (int i=0; i < 0x10; ++i) {
//        DBG_RT("%02x", KernelData[patchLocation1 + i]);
//      }
//      DBG_RT("\n");
      KernelData[patchLocation1] = 0xEB;
      KernelData[patchLocation1 + 1] = 0x0A;
    } else {
      DBG_RT("=> pattern not good\n");
//      for (int i=0; i < 0x10; ++i) {
//        DBG_RT("%02x", KernelData[patchLocation1 + i]);
//     }
//      DBG_RT("\n");
    }
  }
  UINTN symbol2 = searchProc("xcpm_SMT_scope_msrs"_XS8);
  patchLocation1 = FindRelative32(KernelData, procLocation, 0x200, symbol2);
  if (patchLocation1 != 0) {
    DBG_RT("=> xcpm_SMT_scope_msrs found at %llx\n", patchLocation1);
    if (KernelData[patchLocation1 + 7] == 0xE8) {
      DBG_RT("=> SMT patch applied\n");
      KernelData[patchLocation1] = 0xEB;
      KernelData[patchLocation1 + 1] = 0x0A;
    } else {
      DBG_RT("=> pattern not good\n");
    }
  }

  Stall(10000000);

#else
  // Credits to RehabMan for the kernel patch information
  // new way by RehabMan 2017-08-13
  // cleanup by Sherlocks 2020-03-23
#define CompareWithMask(x,m,c) (((x) & (m)) == (c))
  //TODO - remake using CompareMemMask

  UINT64* Ptr = (UINT64*)KernelData;
  UINT64* End = Ptr + 0x1000000/sizeof(UINT64);
  if (Ptr == NULL) {
    return false;
  }

  

  for (; Ptr < End; Ptr += 2) {
    // check for xcpm_scope_msr common 0xE2 prologue
    // E2000000 XX000000 00000000 00000000 00040000 00000000
    // 10.8/10.9: 02,0C,10
    // E2000000 XXXX0000 00000000 00000000 0F040000 00000000
    // 10.10: 0200,4C00,9001, 10.11: 0200,4C00,9013, 10.12: 4C00,9033, 10.13-10.15.3: 4C00,9033,0040
    // E2000000 XXXXXX00 00000000 00000000 0F040000 00000000
    // 10.15.4+: 4C0000,903306,004000

    // E2000000 XXXXXXXX 00000000 00000000 XX040000 00000000
    // safe pattern for next macOS
    if (CompareWithMask(Ptr[0], 0x00000000FFFFFFFF, 0x00000000000000E2) && 0 == Ptr[1] &&
        CompareWithMask(Ptr[2], 0xFFFFFFFFFFFFFF00, 0x0000000000000400)) {
      // 10.8 - 10.12
      // 0700001E 00000000 00000000 00000000 00000000 00000000
      // 0500001E 00000000 00000000 00000000 00000000 00000000
      // 0800007E 00000000 00000000 00000000 00000000 00000000
      // 10.13+
      // 0500001E 00000000 00000000 00000000 00000000 00000000
      // 0800007E 00000000 00000000 00000000 00000000 00000000
      // 0300007E 00000000 00000000 00000000 00000000 00000000

      // XX00001E 00000000 00000000 00000000 00000000 00000000
      if (CompareWithMask(Ptr[3], 0xFFFFFFFFFFFFFF00, 0x000000001E000000) && 0 == Ptr[4] && 0 == Ptr[5]) {
        // zero out 0xE2 MSR and CPU mask
        Ptr[0] = 0;
        DBG_RT("Kernel power management: entry 1E found and patched\n");
      // XX00007E 00000000 00000000 00000000 00000000 00000000
      } else if (CompareWithMask(Ptr[3], 0xFFFFFFFFFFFFFF00, 0x000000007E000000) && 0 == Ptr[4] && 0 == Ptr[5]) {
        // zero out 0xE2 MSR and CPU mask
        Ptr[0] = 0;
        DBG_RT("Kernel power management: entry 7E found and patched\n");
      }
    }
  }
    
  if (KernelAndKextPatches.KPDebug) {
    gBS->Stall(3000000);
  }
#endif
  return true;
}

const UINT8 PanicNoKextDumpFind[]    = {0x00, 0x25, 0x2E, 0x2A, 0x73, 0x00};
//STATIC UINT8 PanicNoKextDumpReplace[6] = {0x00, 0x00, 0x2E, 0x2A, 0x73, 0x00};

XBool LOADER_ENTRY::KernelPanicNoKextDump()
{
  INT32      patchLocation;
  patchLocation = FindBin(KernelData, 0xF00000, PanicNoKextDumpFind, 6);
  if (patchLocation > 0) {
    KernelData[patchLocation + 1] = 0;
    return true;
  }
  return false;
}

XBool LOADER_ENTRY::KernelLapicPatch_64()
{
  // Credits to donovan6000 and Sherlocks for providing the lapic kernel patch source used to build this function

  UINT8 *bytes = KernelData;
  UINTN      patchLocation1 = 0, patchLocation2 = 0;
  UINT32      i, y;

  DBG_RT( "Looking for Lapic panic call (64-bit) Start\n");
  //Slice - symbolic method
  //start at lapic_interrupt ffffff80004e4950 => @2e4950
  //    found pattern: 1
  // address: 002e4a2f
  // bytes:658b04251c0000003b058bb97b00
  // call _panic -> change to nop {90,90,90,90,90}
  if ( macOSVersion >= MacOsVersion("10.10"_XS8) ) {
    UINTN procAddr = searchProc("lapic_interrupt"_XS8);
    patchLocation1 = searchProc("_panic"_XS8);
    patchLocation2 = FindRelative32(KernelData, procAddr, 0x140, patchLocation1);
    if (patchLocation2 != 0) {
      KernelData[patchLocation2 - 5] = 0xEB;
      KernelData[patchLocation2 - 4] = 0x03;
      DBG_RT( "Lapic panic patched\n");
      return true;
    }
  }
  //else old method

  for (i = 0; i < 0x1000000; i++) {
    if (KernelData[i+0] == 0x65 && KernelData[i+1] == 0x8B && KernelData[i+2] == 0x04 && KernelData[i+3] == 0x25 &&
        KernelData[i+4] == 0x3C && KernelData[i+5] == 0x00 && KernelData[i+6] == 0x00 && KernelData[i+7] == 0x00 &&
        KernelData[i+45] == 0x65 && KernelData[i+46] == 0x8B && KernelData[i+47] == 0x04 && KernelData[i+48] == 0x25 &&
        KernelData[i+49] == 0x3C && KernelData[i+50] == 0x00 && KernelData[i+51] == 0x00 && KernelData[i+52] == 0x00) {
      patchLocation1 = i+40;
      DBG_RT( "Found Lapic panic (10.6) at 0x%08llx\n", patchLocation1);
      break;
    } else if (KernelData[i+0]  == 0x65 && KernelData[i+1]  == 0x8B && KernelData[i+2]  == 0x04 && KernelData[i+3]  == 0x25 &&
               KernelData[i+4]  == 0x14 && KernelData[i+5]  == 0x00 && KernelData[i+6]  == 0x00 && KernelData[i+7]  == 0x00 &&
               KernelData[i+35] == 0x65 && KernelData[i+36] == 0x8B && KernelData[i+37] == 0x04 && KernelData[i+38] == 0x25 &&
               KernelData[i+39] == 0x14 && KernelData[i+40] == 0x00 && KernelData[i+41] == 0x00 && KernelData[i+42] == 0x00) {
      patchLocation1 = i+30;
      DBG_RT( "Found Lapic panic (10.7 - 10.8) at 0x%08llx\n", patchLocation1);
      break;
    } else if (KernelData[i+0] == 0x65 && KernelData[i+1] == 0x8B && KernelData[i+2] == 0x04 && KernelData[i+3] == 0x25 &&
               KernelData[i+4] == 0x1C && KernelData[i+5] == 0x00 && KernelData[i+6] == 0x00 && KernelData[i+7] == 0x00 &&
               KernelData[i+36] == 0x65 && KernelData[i+37] == 0x8B && KernelData[i+38] == 0x04 && KernelData[i+39] == 0x25 &&
               KernelData[i+40] == 0x1C && KernelData[i+41] == 0x00 && KernelData[i+42] == 0x00 && KernelData[i+43] == 0x00) {
      patchLocation1 = i+31;
      DBG_RT( "Found Lapic panic (10.9) at 0x%08llx\n", patchLocation1);
      break;
    // 00 29 C7 78 XX 31 DB 8D 47 FA 83
    } else if (KernelData[i+0] == 0x00 && KernelData[i+1] == 0x29 && KernelData[i+2] == 0xC7 && KernelData[i+3] == 0x78 &&
               //(bytes[i+4] == 0x3F || bytes[i+4] == 0x4F) && // 3F:10.10-10.12/4F:10.13+
               bytes[i+5] == 0x31 && bytes[i+6] == 0xDB && bytes[i+7] == 0x8D && bytes[i+8] == 0x47 &&
               bytes[i+9] == 0xFA && bytes[i+10] == 0x83) {
      DBG_RT( "Found Lapic panic Base (10.10 - recent macOS)\n");
      for (y = i; y < 0x1000000; y++) {
        // Lapic panic patch, by vit9696
        // mov eax, gs:XX
        // cmp eax, cs:_master_cpu
        // 65 8B 04 25 XX 00 00 00 3B 05 XX XX XX 00
        if (bytes[y+0] == 0x65 && bytes[y+1] == 0x8B && bytes[y+2] == 0x04 && bytes[y+3] == 0x25 &&
            //(bytes[y+4] == 0x1C || bytes[y+4] == 0x18) && // 1C:10.10-10.15.3/18:10.15.4+
            bytes[y+5] == 0x00 && bytes[y+6] == 0x00 && bytes[y+7] == 0x00 &&
            bytes[y+8] == 0x3B && bytes[y+9] == 0x05 && bytes[y+13] == 0x00) {
          patchLocation1 = y;
          DBG_RT( "Found Lapic panic (10.10 - recent macOS) at 0x%08llx\n", patchLocation1);
          break;
        }
      }
      break;
    }
  }

  if (!patchLocation1) {
    DBG_RT( "Can't find Lapic panic, kernel patch aborted.\n");
    return false;
  }
  
  // Already patched?  May be running a non-vanilla kernel already?
  if (bytes[patchLocation1 + 0] == 0x90 && bytes[patchLocation1 + 1] == 0x90 &&
      bytes[patchLocation1 + 2] == 0x90 && bytes[patchLocation1 + 3] == 0x90 &&
      bytes[patchLocation1 + 4] == 0x90) {
    DBG_RT( "Lapic panic already patched, kernel file (10.6 - 10.9) manually patched?\n");
    return false;
  } else if (bytes[patchLocation1 + 0] == 0x31 && bytes[patchLocation1 + 1] == 0xC0 &&
             bytes[patchLocation1 + 2] == 0x90 && bytes[patchLocation1 + 3] == 0x90) {
    DBG_RT( "Lapic panic already patched, kernel file (10.10 - recent macOS) manually patched?\n");
    return false;
  } else {
    if (bytes[patchLocation1 + 8] == 0x3B && bytes[patchLocation1 + 9] == 0x05 &&
        bytes[patchLocation1 + 13] == 0x00) {
      // 65 8B 04 25 XX 00 00 00 3B 05 XX XX XX 00
      // 31 C0 90 90 90 90 90 90 90 90 90 90 90 90
      DBG_RT( "Patched Lapic panic (10.10 - recent macOS)\n");
      bytes[patchLocation1 + 0] = 0x31;
      bytes[patchLocation1 + 1] = 0xC0;
      for (i = 2; i < 14; i++) {
        bytes[patchLocation1 + i] = 0x90;
      }

      for (i = 0; i < 0x1000000; i++) {
        // 00 29 C7 78 XX 31 DB 8D 47 FA 83
        if (bytes[i+0] == 0x00 && bytes[i+1] == 0x29 && bytes[i+2] == 0xC7 && bytes[i+3] == 0x78 &&
            //(bytes[i+4] == 0x3F || bytes[i+4] == 0x4F) && // 3F:10.10-10.12/4F:10.13+
            bytes[i+5] == 0x31 && bytes[i+6] == 0xDB && bytes[i+7] == 0x8D && bytes[i+8] == 0x47 &&
            bytes[i+9] == 0xFA && bytes[i+10] == 0x83) {
          DBG_RT( "Found Lapic panic master Base (10.10 - recent macOS)\n");
          for (y = i; y < 0x1000000; y++) {
            // Lapic panic master patch, by vit9696
            // cmp cs:_debug_boot_arg, 0
            // E8 XX XX FF FF 83 XX XX XX XX 00 00
            if (bytes[y+0] == 0xE8 && bytes[y+3] == 0xFF && bytes[y+4] == 0xFF &&
                bytes[y+5] == 0x83 && bytes[y+10] == 0x00 && bytes[y+11] == 0x00) {
              patchLocation2 = y;
              DBG_RT( "Found Lapic panic master (10.10 - recent macOS) at 0x%08llx\n", patchLocation2);
              break;
            }
          }
          break;
        }
      }
        
      if (!patchLocation2) {
        DBG_RT( "Can't find Lapic panic master (10.10 - recent macOS), kernel patch aborted.\n");
        return false;
      }
        
      // Already patched? May be running a non-vanilla kernel already?
      if (bytes[patchLocation2 + 5] == 0x31 && bytes[patchLocation2 + 6] == 0xC0) {
        DBG_RT( "Lapic panic master already patched, kernel file (10.10 - recent macOS) manually patched?\n");
        return false;
      } else {
        DBG_RT( "Patched Lapic panic master (10.10 - recent macOS)\n");
        // E8 XX XX FF FF 83 XX XX XX XX 00 00
        // E8 XX XX FF FF 31 C0 90 90 90 90 90 xor eax,eax; nop; nop;....
        bytes[patchLocation2 + 5] = 0x31;
        bytes[patchLocation2 + 6] = 0xC0;
        for (i = 7; i < 12; i++) {
          bytes[patchLocation2 + i] = 0x90;
        }
      }
    } else {
      DBG_RT( "Patched Lapic panic (10.6 - 10.9)\n");
      for (i = 0; i < 5; i++) {
        bytes[patchLocation1 + i] = 0x90;
      }
    }
  }
    
//  if (KernelAndKextPatches.KPDebug) {
    Stall(3000000);
//  }
    
  return true;
}

XBool LOADER_ENTRY::KernelLapicPatch_32()
{
  // Credits to donovan6000 and Sherlocks for providing the lapic kernel patch source used to build this function

  UINT8       *bytes = KernelData;
  UINT32      patchLocation = 0;
  UINT32      i;

  DBG_RT( "Looking for Lapic panic call (32-bit) Start\n");

  for (i = 0; i < 0x1000000; i++) {
    if (bytes[i+0]  == 0x65 && bytes[i+1]  == 0xA1 && bytes[i+2]  == 0x0C && bytes[i+3]  == 0x00 &&
        bytes[i+4]  == 0x00 && bytes[i+5]  == 0x00 &&
        bytes[i+30] == 0x65 && bytes[i+31] == 0xA1 && bytes[i+32] == 0x0C && bytes[i+33] == 0x00 &&
        bytes[i+34] == 0x00 && bytes[i+35] == 0x00) {
      patchLocation = i+25;
      DBG_RT( "Found Lapic panic at 0x%08x\n", patchLocation);
      break;
    }
  }

  if (!patchLocation) {
    DBG_RT( "Can't find Lapic panic, kernel patch aborted.\n");
    return false;
  }

  // Already patched?  May be running a non-vanilla kernel already?

  if (bytes[patchLocation + 0] == 0x90 && bytes[patchLocation + 1] == 0x90 &&
      bytes[patchLocation + 2] == 0x90 && bytes[patchLocation + 3] == 0x90 &&
      bytes[patchLocation + 4] == 0x90) {
    DBG_RT( "Lapic panic already patched, kernel file manually patched?\n");
    return false;
  } else {
    DBG_RT( "Patched Lapic panic (32-bit)\n");
    for (i = 0; i < 5; i++) {
      bytes[patchLocation + i] = 0x90;
    }
  }

//  if (KernelAndKextPatches.KPDebug) {
    Stall(3000000);
//  }

  return true;
}

//
// syscl - EnableExtCpuXCPM(): enable extra(unsupport) Cpu XCPM function
// PowerManagement that will be enabled on:
// SandyBridge-E, Ivy Bridge, Ivy Bridge-E, Haswell Celeron/Pentium, Haswell-E, Broadwell-E, ...
// credit Pike R.Alpha, stinga11, syscl
//
//XBool (*EnableExtCpuXCPM)(void *kernelData);

//
// syscl - applyKernPatch a wrapper for SearchAndReplace() to make the CpuPM patch tidy and clean
//
void LOADER_ENTRY::applyKernPatch(const UINT8 *find, UINTN size, const UINT8 *repl, const CHAR8 *comment)
{
    DBG("Searching %s...\n", comment);
    if (SearchAndReplace(KernelData, KERNEL_MAX_SIZE, find, size, repl, 0)) {
        DBG("Found %s\nApplied patch\n", comment);
    } else {
        DBG("%s no found, patched already?\n", comment);
    }
}

// PMHeart
// Global XCPM patches compatibility
// Currently 10.8.5 - 10.15
//
static inline XBool IsXCPMOSVersionCompat(const MacOsVersion& macOSVersion)
{
  return macOSVersion >= MacOsVersion("10.8.5"_XS8)  &&  macOSVersion < MacOsVersion("11.1.0"_XS8);
}

//
// Enable Unsupported CPU PowerManagement
//
// syscl - SandyBridgeEPM(): enable PowerManagement on SandyBridge-E
//
XBool LOADER_ENTRY::SandyBridgeEPM()
{
    // note: a dummy function that made patches consistency
    return true;
}

//
// syscl - Enable Haswell-E XCPM
// Hex data provided and polished (c) PMheart, idea (c) Pike R.Alpha
//
XBool LOADER_ENTRY::HaswellEXCPM()
{
  DBG("HaswellEXCPM() ===>\n");
  UINT8       *kern = KernelData;
  XString8     comment;
 // UINT32      i;
  UINTN      patchLocation;
//  UINT64     os_version = AsciiOSVersionToUint64(macOSVersion);

  // check OS version suit for patches
  if (!IsXCPMOSVersionCompat(macOSVersion)) {
    DBG("HaswellEXCPM(): Unsupported macOS.\n");
    DBG("HaswellEXCPM() <===false\n");
    return false;
  }

  // _cpuid_set_info
  comment = "_cpuid_set_info"_XS8;
  if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.8.5"_XS8)) {
    // 10.8.5
    const UINT8 find[] = { 0x83, 0xF8, 0x3C, 0x74, 0x2D };
    const UINT8 repl[] = { 0x83, 0xF8, 0x3F, 0x74, 0x2D };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.10"_XS8)) {
    // 10.9.x
    const UINT8 find[] = { 0x83, 0xF8, 0x3C, 0x75, 0x07 };
    const UINT8 repl[] = { 0x83, 0xF8, 0x3F, 0x75, 0x07 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.10.1"_XS8)) {
    // 10.10 - 10.10.1
    const UINT8 find[] = { 0x74, 0x11, 0x83, 0xF8, 0x3C };
    const UINT8 repl[] = { 0x74, 0x11, 0x83, 0xF8, 0x3F };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } // 10.10.2+: native support reached, no need to patch

  // _xcpm_bootstrap
  comment = "_xcpm_bootstrap"_XS8;
  if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.8.5"_XS8)) {
    // 10.8.5
    const UINT8 find[] = { 0x83, 0xFB, 0x3C, 0x75, 0x54 };
    const UINT8 repl[] = { 0x83, 0xFB, 0x3F, 0x75, 0x54 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.10"_XS8)) {
    // 10.9.x
    const UINT8 find[] = { 0x83, 0xFB, 0x3C, 0x75, 0x68 };
    const UINT8 repl[] = { 0x83, 0xFB, 0x3F, 0x75, 0x68 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.10.2"_XS8)) {
    // 10.10 - 10.10.2
    const UINT8 find[] = { 0x83, 0xFB, 0x3C, 0x75, 0x63 };
    const UINT8 repl[] = { 0x83, 0xFB, 0x3F, 0x75, 0x63 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.10.5"_XS8)) {
    // 10.10.3 - 10.10.5
    const UINT8 find[] = { 0x83, 0xC3, 0xC6, 0x83, 0xFB, 0x0D };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC3, 0x83, 0xFB, 0x0D };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.11"_XS8)) {
    // 10.11 DB/PB - 10.11.0
    const UINT8 find[] = { 0x83, 0xC3, 0xC6, 0x83, 0xFB, 0x0D };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC3, 0x83, 0xFB, 0x0D };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.11.6"_XS8)) {
    // 10.11.1 - 10.11.6
    const UINT8 find[] = { 0x83, 0xC3, 0xBB, 0x83, 0xFB, 0x09 };
    const UINT8 repl[] = { 0x83, 0xC3, 0xB8, 0x83, 0xFB, 0x09 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else {
    UINTN procLocation = searchProc(comment);
    UINTN featureCall = searchProc("_cpuid_features"_XS8);
    UINTN place = FindRelative32(KernelData, procLocation, 0x100, featureCall);
    for (UINTN i = 10; i < 20; ++i) {
      if (KernelData[place + i] == 0xC4) {
        KernelData[place + i] = 0xC1;
        if (KernelData[(place + i) - 5] == 0x3B) {
          KernelData[(place + i) - 5] = 0x00;
        }
        break;
      }
    }

    /*if (macOSVersion.notEmpty() && macOSVersion <= AsciiOSVersionToUint64("10.12.5")) {
    // 10.12 - 10.12.5
    const UINT8 find[] = { 0x83, 0xC3, 0xC4, 0x83, 0xFB, 0x22 };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC1, 0x83, 0xFB, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.13")) {
    // 10.12.6
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x83, 0xF8, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC1, 0x83, 0xF8, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
    // PMheart: attempt to add 10.14 compatibility
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.15")) {
    // 10.13/10.14
    const UINT8 find[] = { 0x89, 0xD8, 0x04, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x89, 0xD8, 0x04, 0xC1, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
    // PMheart: attempt to add 10.15 compatibility
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.15.4")) {
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC1, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.16")) {
    // vector sigma: 10.15.5 Beta 2 build 19F62f and 10.15.4 build 19E287
    const UINT8 find[] = { 0x3B, 0x7E, 0x2E, 0x80, 0xC3, 0xC4, 0x80, 0xFB, 0x42 };
    const UINT8 repl[] = { 0x00, 0x7E, 0x2E, 0x80, 0xC3, 0xC1, 0x80, 0xFB, 0x42 };
    applyKernPatch(find, sizeof(find), repl, comment);
     */
  }

  DBG("Searching _xcpm_pkg_scope_msr ...\n");
  comment = "_xcpm_pkg_scope_msrs"_XS8;
  if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.8.5"_XS8)) {
    // 10.8.5
    const UINT8 find[] = {
      0x48, 0x8D, 0x3D, 0x02, 0x71, 0x55, 0x00, 0xBE,
      0x07, 0x00, 0x00, 0x00, 0xEB, 0x1F, 0x48, 0x8D,
      0x3D, 0xF4, 0x70, 0x55, 0x00, 0xBE, 0x07, 0x00,
      0x00, 0x00, 0x31, 0xD2, 0xE8, 0x28, 0x02, 0x00, 0x00
    };
    const UINT8 repl[] = {
      0x48, 0x8D, 0x3D, 0x02, 0x71, 0x55, 0x00, 0xBE,
      0x07, 0x00, 0x00, 0x00, 0x90, 0x90, 0x48, 0x8D,
      0x3D, 0xF4, 0x70, 0x55, 0x00, 0xBE, 0x07, 0x00,
      0x00, 0x00, 0x31, 0xD2, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.10"_XS8)) {
    // 10.9.x
    const UINT8 find[] = { 0xBE, 0x07, 0x00, 0x00, 0x00, 0x74, 0x13, 0x31, 0xD2, 0xE8, 0x5F, 0x02, 0x00, 0x00 };
    const UINT8 repl[] = { 0xBE, 0x07, 0x00, 0x00, 0x00, 0x90, 0x90, 0x31, 0xD2, 0x90, 0x90, 0x90, 0x90, 0x90 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else {
    // 10.10+
/*    patchLocation = 0; // clean out the value just in case
    for (i = 0; i < 0x1000000; i++) {
      if (kern[i+0] == 0xBE && kern[i+1] == 0x07 && kern[i+2] == 0x00 && kern[i+3] == 0x00 &&
          kern[i+4] == 0x00 && kern[i+5] == 0x31 && kern[i+6] == 0xD2 && kern[i+7] == 0xE8) {
        patchLocation = i+7;
        DBG("Found _xcpm_pkg_scope_msr\n");
        break;
      }
    } */
    UINTN procLocation = searchProc("xcpm_init"_XS8);
    UINTN symbol1 = searchProc("_xcpm_pkg_scope_msrs"_XS8);
    patchLocation = FindRelative32(KernelData, procLocation, 0x100, symbol1);


    if (patchLocation) {
      for (UINTN i = 7; i < 12; i++) {
        kern[patchLocation+i] = 0x90;
      }
      DBG("Applied _xcpm_pkg_scope_msr patch\n");
    } else {
      DBG("_xcpm_pkg_scope_msr not found, patch aborted\n");
      DBG("HaswellEXCPM() <===false\n");
      return false;
    }
  }

  DBG("HaswellEXCPM() <===\n");
  return true;
}

//
// Enable Broadwell-E/EP PowerManagement on 10.12+ by syscl
//
XBool LOADER_ENTRY::BroadwellEPM()
{
  DBG("BroadwellEPM() ===>\n");

  UINT32      i;
  UINTN       patchLocation;
//  UINT64      os_version = AsciiOSVersionToUint64(macOSVersion);

  // check OS version suit for patches
  if (!IsXCPMOSVersionCompat(macOSVersion)) {
    DBG("BroadwellEPM(): Unsupported macOS.\n");
    DBG("BroadwellEPM() <===false\n");
    return false;
  }

  KernelAndKextPatches.FakeCPUID = (UINT32)(macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.10.3"_XS8) ? 0x0306C0 : 0x040674);
  KernelCPUIDPatch();

  DBG("Searching _xcpm_pkg_scope_msr ...\n");
  // proc: _xcpm_init @4687b0
  // ffffff8000468825 488D3D54527F00                  lea        rdi, qword [ds:_xcpm_pkg_scope_msrs]
  // ffffff800046882c BE07000000                      mov        esi, 0x7
  // ffffff8000468831 31D2                            xor        edx, edx
  // ffffff8000468833 E838FDFFFF                      call       sub_ffffff8000468570
  if (macOSVersion >= MacOsVersion("10.12"_XS8)) {
    // 10.12+
//    patchLocation = 0; // clean out the value just in case
//    for (i = 0; i < 0x1000000; i++) {
//      if (kern[i+0] == 0xBE && kern[i+1] == 0x07 && kern[i+2] == 0x00 && kern[i+3] == 0x00 &&
//          kern[i+4] == 0x00 && kern[i+5] == 0x31 && kern[i+6] == 0xD2 && kern[i+7] == 0xE8) {
//        patchLocation = i+7;
//       DBG("Found _xcpm_pkg_scope_msr\n");
//        break;
//      }
    UINTN procLocation = searchProc("xcpm_init"_XS8);
    UINTN symbol1 = searchProc("_xcpm_pkg_scope_msrs"_XS8);
    patchLocation = FindRelative32(KernelData, procLocation, 0x100, symbol1);

 
    if (patchLocation) {
      for (i = 7; i < 12; i++) {
        KernelData[patchLocation+i] = 0x90;
      }
      DBG("Applied _xcpm_pkg_scope_msr patch\n");
    } else {
      DBG("_xcpm_pkg_scope_msr not found, patch aborted\n");
      DBG("BroadwellEPM() <===false\n");
      return false;
    }
  }

  DBG("BroadwellEPM() <===\n");
  return true;
}
//
// syscl - this patch provides XCPM support for Haswell low-end(HSWLowEnd) and platforms later than Haswell
// implemented by syscl
// credit also Pike R.Alpha, stinga11, Sherlocks, vit9696
//
XBool LOADER_ENTRY::HaswellLowEndXCPM()
{
  DBG("HaswellLowEndXCPM() ===>\n");
//  UINT64      os_version = AsciiOSVersionToUint64(macOSVersion);
  XString8    comment;

  // check OS version suit for patches
  if (!IsXCPMOSVersionCompat(macOSVersion)) {
    DBG("HaswellLowEndXCPM(): Unsupported macOS.\n");
    DBG("HaswellLowEndXCPM() <===false\n");
    return false;
  }

  KernelAndKextPatches.FakeCPUID = (UINT32)(0x0306A0);    // correct FakeCPUID
  KernelCPUIDPatch();

  // 10.8.5 - 10.11.x no need the following kernel patches on Haswell Celeron/Pentium
  if (macOSVersion >= MacOsVersion("10.8.5"_XS8) && macOSVersion < MacOsVersion("10.12"_XS8)) {
    DBG("HaswellLowEndXCPM() <===\n");
    return true;
  }

  // _xcpm_idle //this is a part of KernelPM
/*  if (use_xcpm_idle) {
    DBG("HWPEnable - ON.\n");
    comment = "_xcpm_idle";
    const UINT8 find[] = { 0xB9, 0xE2, 0x00, 0x00, 0x00, 0x0F, 0x30 };
    const UINT8 repl[] = { 0xB9, 0xE2, 0x00, 0x00, 0x00, 0x90, 0x90 };
    applyKernPatch(find, sizeof(find), repl, comment);
  }
*/
  comment = "_xcpm_bootstrap"_XS8;
  UINTN procLocation = searchProc(comment);
  UINTN featureCall = searchProc("_cpuid_features"_XS8);
  UINTN place = FindRelative32(KernelData, procLocation, 0x100, featureCall);
  for (UINTN i = 10; i < 20; ++i) {
    if (KernelData[place + i] == 0xC4) {
      KernelData[place + i] = 0xC6;
      if (KernelData[(place + i) - 5] == 0x3B) {
        KernelData[(place + i) - 5] = 0x00;
      }
      break;
    }
  }
  /*
  if (macOSVersion.notEmpty() && macOSVersion <= AsciiOSVersionToUint64("10.12.5")) {
    // 10.12 - 10.12.5
    const UINT8 find[] = { 0x83, 0xC3, 0xC4, 0x83, 0xFB, 0x22 };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC6, 0x83, 0xFB, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.13")) {
    // 10.12.6
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x83, 0xF8, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC6, 0x83, 0xF8, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.15")) {
    // 10.13/10.14
    //    ; Basic Block Input Regs: rbx -  Killed Regs: rax
    //    ffffff80004fa0f7 89D8                            mov        eax, ebx
    //    ffffff80004fa0f9 04C4                            add        al, 0xc4
    //    ffffff80004fa0fb 3C22                            cmp        al, 0x22
    //    ffffff80004fa0fd 7722                            jnbe       0xffffff80004fa121
    const UINT8 find[] = { 0x89, 0xD8, 0x04, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x89, 0xD8, 0x04, 0xC6, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
    // PMheart: attempt to add 10.15 compatibility
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.15.4")) {
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC6, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.16")) {
    // vector sigma: 10.15.5 Beta 2 build 19F62f and 10.15.4 build 19E287
    const UINT8 find[] = { 0x3B, 0x7E, 0x2E, 0x80, 0xC3, 0xC4, 0x80, 0xFB, 0x42 };
    const UINT8 repl[] = { 0x00, 0x7E, 0x2E, 0x80, 0xC3, 0xC6, 0x80, 0xFB, 0x42 };
    applyKernPatch(find, sizeof(find), repl, comment);
  }
*/
  comment = "_cpuid_set_info_rdmsr"_XS8;
  // PMheart: bytes seem stable as of 10.12
  if (macOSVersion >= MacOsVersion("10.12"_XS8)) {
    // 10.12+
    const UINT8 find[] = { 0xB9, 0xA0, 0x01, 0x00, 0x00, 0x0F, 0x32 };
    const UINT8 repl[] = { 0xB9, 0xA0, 0x01, 0x00, 0x00, 0x31, 0xC0 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  }

  DBG("HaswellLowEndXCPM() <===\n");
  return true;
}

//
// this patch provides XCPM support for Ivy Bridge. by PMheart
//
XBool LOADER_ENTRY::KernelIvyBridgeXCPM()
{
  XString8      comment;
//  UINT32      i;
  UINTN       patchLocation;
//  UINT64      os_version = AsciiOSVersionToUint64(macOSVersion);

  // check whether Ivy Bridge
  if (gCPUStructure.Model != CPU_MODEL_IVY_BRIDGE) {
    DBG("KernelIvyBridgeXCPM(): Unsupported platform.\nRequires Ivy Bridge, aborted\n");
    DBG("KernelIvyBridgeXCPM() <===false\n");
    return false;
  }

  // check OS version suit for patches
  // PMheart: attempt to add 10.14 compatibility
  if (!IsXCPMOSVersionCompat(macOSVersion)) {
    DBG("KernelIvyBridgeXCPM():Unsupported macOS.\n");
    DBG("KernelIvyBridgeXCPM() <===false\n");
    return false;
  } else if (macOSVersion >= MacOsVersion("10.8.5"_XS8) && macOSVersion < MacOsVersion("10.12"_XS8)) {
    // 10.8.5 - 10.11.x no need the following kernel patches on Ivy Bridge - we just use -xcpm boot-args
    DBG("KernelIvyBridgeXCPM() <===\n");
    return true;
  }

  DBG("Searching _xcpm_pkg_scope_msr ...\n");
  if (macOSVersion >= MacOsVersion("10.12"_XS8)) {
    // 10.12+
/*    patchLocation = 0; // clean out the value just in case
    for (i = 0; i < 0x1000000; i++) {
      if (kern[i+0] == 0xBE && kern[i+1] == 0x07 && kern[i+2] == 0x00 && kern[i+3] == 0x00 &&
          kern[i+4] == 0x00 && kern[i+5] == 0x31 && kern[i+6] == 0xD2 && kern[i+7] == 0xE8) {
        patchLocation = i+7;
        DBG("Found _xcpm_pkg_scope_msr\n");
        break;
      }
    } */
    UINTN procLocation = searchProc("xcpm_init"_XS8);
    UINTN symbol1 = searchProc("_xcpm_pkg_scope_msrs"_XS8);
    patchLocation = FindRelative32(KernelData, procLocation, 0x100, symbol1);


    if (patchLocation) {
      for (int i = 7; i < 12; i++) {
        KernelData[patchLocation+i] = 0x90;
      }
      DBG("Applied _xcpm_pkg_scope_msr patch\n");
    } else {
      DBG("_xcpm_pkg_scope_msr not found, patch aborted\n");
      DBG("KernelIvyBridgeXCPM() <===false\n");
      return false;
    }
  }

  comment = "_xcpm_bootstrap"_XS8;
  UINTN procLocation = searchProc(comment);
  UINTN featureCall = searchProc("_cpuid_features"_XS8);
  UINTN place = FindRelative32(KernelData, procLocation, 0x100, featureCall);
  for (UINTN i = 10; i < 20; ++i) {
    if (KernelData[place + i] == 0xC4) {
      KernelData[place + i] = 0xC6;
      if (KernelData[(place + i) - 5] == 0x3B) {
        KernelData[(place + i) - 5] = 0x00;
      }
      break;
    }
  }
/*
  if (macOSVersion.notEmpty() && macOSVersion <= AsciiOSVersionToUint64("10.12.5")) {
    // 10.12 - 10.12.5
    const UINT8 find[] = { 0x83, 0xC3, 0xC4, 0x83, 0xFB, 0x22 };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC6, 0x83, 0xFB, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.13")) {
    // 10.12.6
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x83, 0xF8, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC6, 0x83, 0xF8, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
    // PMheart: attempt to add 10.14 compatibility
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.15")) {
    // 10.13/10.14
    const UINT8 find[] = { 0x89, 0xD8, 0x04, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x89, 0xD8, 0x04, 0xC6, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
    // PMheart: attempt to add 10.15 compatibility
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.15.4")) {
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC6, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.16")) {
    // vector sigma: 10.15.5 Beta 2 build 19F62f and 10.15.4 build 19E287
    const UINT8 find[] = { 0x3B, 0x7E, 0x2E, 0x80, 0xC3, 0xC4, 0x80, 0xFB, 0x42 };
    const UINT8 repl[] = { 0x00, 0x7E, 0x2E, 0x80, 0xC3, 0xC6, 0x80, 0xFB, 0x42 };
    applyKernPatch(find, sizeof(find), repl, comment);
  }
*/
  DBG("KernelIvyBridgeXCPM() <===\n");
  return true;
}

//
// this patch provides XCPM support for Ivy Bridge-E. by PMheart
// attempt to enable XCPM for Ivy-E, still need to test further
//
XBool LOADER_ENTRY::KernelIvyE5XCPM()
{
  UINT8       *kern = (UINT8*)KernelData;
  XString8    comment;
 // UINT32      i;
  UINTN       patchLocation;
//  UINT64      os_version = AsciiOSVersionToUint64(macOSVersion);
  
  // check whether Ivy Bridge-E5
  if (gCPUStructure.Model != CPU_MODEL_IVY_BRIDGE_E5) {
    DBG("KernelIvyE5XCPM(): Unsupported platform.\nRequires Ivy Bridge-E, aborted\n");
    DBG("KernelIvyE5XCPM() <===false\n");
    return false;
  }
  
  // check OS version suit for patches
  // PMheart: attempt to add 10.15 compatibility
  if (!IsXCPMOSVersionCompat(macOSVersion)) {
    DBG("KernelIvyE5XCPM(): Unsupported macOS.\n");
    DBG("KernelIvyE5XCPM() <===false\n");
    return false;
  }
  
  // _cpuid_set_info
  // TODO: should we use FakeCPUID instead?
  comment = "_cpuid_set_info"_XS8;
  if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.8.5"_XS8)) {
    // 10.8.5
    const UINT8 find[] = { 0x83, 0xF8, 0x3C, 0x74, 0x2D };
    const UINT8 repl[] = { 0x83, 0xF8, 0x3E, 0x74, 0x2D };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if ( macOSVersion == MacOsVersion("10.9"_XS8) || macOSVersion == MacOsVersion("10.9.1"_XS8) ) {
    // 10.9.0 - 10.9.1
    const UINT8 find[] = { 0x83, 0xF8, 0x3C, 0x75, 0x07 };
    const UINT8 repl[] = { 0x83, 0xF8, 0x3E, 0x75, 0x07 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } // 10.9.2+: native support reached, no need to patch
  
  // _xcpm_pkg_scope_msrs
  DBG("Searching _xcpm_pkg_scope_msrs ...\n");
  comment = "_xcpm_pkg_scope_msrs"_XS8;
  if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.8.5"_XS8)) {
    // 10.8.5
    const UINT8 find[] = {
      0x48, 0x8D, 0x3D, 0x02, 0x71, 0x55, 0x00, 0xBE,
      0x07, 0x00, 0x00, 0x00, 0xEB, 0x1F, 0x48, 0x8D,
      0x3D, 0xF4, 0x70, 0x55, 0x00, 0xBE, 0x07, 0x00,
      0x00, 0x00, 0x31, 0xD2, 0xE8, 0x28, 0x02, 0x00, 0x00
    };
    const UINT8 repl[] = {
      0x48, 0x8D, 0x3D, 0x02, 0x71, 0x55, 0x00, 0xBE,
      0x07, 0x00, 0x00, 0x00, 0x90, 0x90, 0x48, 0x8D,
      0x3D, 0xF4, 0x70, 0x55, 0x00, 0xBE, 0x07, 0x00,
      0x00, 0x00, 0x31, 0xD2, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.10"_XS8)) {
    // 10.9.x
    const UINT8 find[] = { 0xBE, 0x07, 0x00, 0x00, 0x00, 0x74, 0x13, 0x31, 0xD2, 0xE8, 0x5F, 0x02, 0x00, 0x00 };
    const UINT8 repl[] = { 0xBE, 0x07, 0x00, 0x00, 0x00, 0x90, 0x90, 0x31, 0xD2, 0x90, 0x90, 0x90, 0x90, 0x90 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else {
    // 10.10+
//    patchLocation = 0; // clean out the value just in case
    UINTN procLocation = searchProc("xcpm_init"_XS8);
    UINTN symbol1 = searchProc(comment);
    patchLocation = FindRelative32(kern, procLocation, 0x100, symbol1);

/*    for (i = 0; i < 0x1000000; i++) {
      if (kern[i+0] == 0xBE && kern[i+1] == 0x07 && kern[i+2] == 0x00 && kern[i+3] == 0x00 &&
          kern[i+4] == 0x00 && kern[i+5] == 0x31 && kern[i+6] == 0xD2 && kern[i+7] == 0xE8) {
        patchLocation = i+7;
        DBG("Found _xcpm_pkg_scope_msr\n");
        break;
      }
    } */
    
    if (patchLocation) {
      for (int i = 7; i < 12; i++) {
        kern[patchLocation+i] = 0x90;
      }
      DBG("Applied _xcpm_pkg_scope_msr patch\n");
    } else {
//      DBG("_xcpm_pkg_scope_msr not found, patch aborted\n");
      DBG("KernelIvyE5XCPM() <===false\n");
      return false;
    }
  }
  
  // _xcpm_bootstrap
  comment = "_xcpm_bootstrap"_XS8;
  if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.8.5"_XS8)) {
    // 10.8.5
    const UINT8 find[] = { 0x83, 0xFB, 0x3C, 0x75, 0x54 };
    const UINT8 repl[] = { 0x83, 0xFB, 0x3E, 0x75, 0x54 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.10"_XS8)) {
    // 10.9.x
    const UINT8 find[] = { 0x83, 0xFB, 0x3C, 0x75, 0x68 };
    const UINT8 repl[] = { 0x83, 0xFB, 0x3E, 0x75, 0x68 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.10.2"_XS8)) {
    // 10.10 - 10.10.2
    const UINT8 find[] = { 0x83, 0xFB, 0x3C, 0x75, 0x63 };
    const UINT8 repl[] = { 0x83, 0xFB, 0x3E, 0x75, 0x63 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.10.5"_XS8)) {
    // 10.10.3 - 10.10.5
    const UINT8 find[] = { 0x83, 0xC3, 0xC6, 0x83, 0xFB, 0x0D };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC4, 0x83, 0xFB, 0x0D };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.11"_XS8)) {
    // 10.11 DB/PB - 10.11.0
    const UINT8 find[] = { 0x83, 0xC3, 0xC6, 0x83, 0xFB, 0x0D };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC4, 0x83, 0xFB, 0x0D };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.11.6"_XS8)) {
    // 10.11.1 - 10.11.6
    const UINT8 find[] = { 0x83, 0xC3, 0xBB, 0x83, 0xFB, 0x09 };
    const UINT8 repl[] = { 0x83, 0xC3, 0xB9, 0x83, 0xFB, 0x09 };
    applyKernPatch(find, sizeof(find), repl, comment.c_str());
  } else {

    UINTN procLocation = searchProc(comment);
    UINTN featureCall = searchProc("_cpuid_features"_XS8);
    UINTN place = FindRelative32(KernelData, procLocation, 0x100, featureCall);
    for (UINTN i = 10; i < 20; ++i) {
      if (KernelData[place + i] == 0xC4) {
        KernelData[place + i] = 0xC1;
        if (KernelData[(place + i) - 5] == 0x3B) {
          KernelData[(place + i) - 5] = 0x00;
        }
        break;
      }
    }

/*    if (macOSVersion.notEmpty() && macOSVersion <= MacOsVersion("10.12.5")) {
    // 10.12 - 10.12.5
    const UINT8 find[] = { 0x83, 0xC3, 0xC4, 0x83, 0xFB, 0x22 };
    const UINT8 repl[] = { 0x83, 0xC3, 0xC2, 0x83, 0xFB, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.13")) {
    // 10.12.6
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x83, 0xF8, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC2, 0x83, 0xF8, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  // PMheart: attempt to add 10.14 compatibility
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.15")) {
    // 10.13/10.14
    const UINT8 find[] = { 0x89, 0xD8, 0x04, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x89, 0xD8, 0x04, 0xC1, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  // PMheart: attempt to add 10.15 compatibility
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.15.4")) {
    const UINT8 find[] = { 0x8D, 0x43, 0xC4, 0x3C, 0x22 };
    const UINT8 repl[] = { 0x8D, 0x43, 0xC1, 0x3C, 0x22 };
    applyKernPatch(find, sizeof(find), repl, comment);
  } else if (macOSVersion.notEmpty() && macOSVersion < MacOsVersion("10.16")) {
    // vector sigma: 10.15.5 Beta 2 build 19F62f and 10.15.4 build 19E287
    const UINT8 find[] = { 0x3B, 0x7E, 0x2E, 0x80, 0xC3, 0xC4, 0x80, 0xFB, 0x42 };
    const UINT8 repl[] = { 0x00, 0x7E, 0x2E, 0x80, 0xC3, 0xC1, 0x80, 0xFB, 0x42 };
    applyKernPatch(find, sizeof(find), repl, comment);
 */
  }
  
  DBG("KernelIvyE5XCPM() <===\n");
  return true;
}
#if 0
void Patcher_SSE3_6(void* kernelData)
{
  UINT8* bytes = (UINT8*)kernelData;
  UINT32 patchLocation1 = 0;
  UINT32 patchLocation2 = 0;
  UINT32 patchlast = 0;
  UINT32 i;
  //UINT32 Length = sizeof(kernelData);

 // DBG("Start find SSE3 address\n");
  i=0;
  //for (i=0;i<Length;i++)
  while(true) {
    if (bytes[i] == 0x66 && bytes[i+1] == 0x0F && bytes[i+2] == 0x6F &&
        bytes[i+3] == 0x44 && bytes[i+4] == 0x0E && bytes[i+5] == 0xF1 &&
        bytes[i-1664-32] == 0x55
        ) {
      patchLocation1 = i-1664-32;
//      DBG("Found SSE3 data address at 0x%08X\n",patchLocation1);
    }

    // hasSSE2+..... title
    if (bytes[i] == 0xE3 && bytes[i+1] == 0x07 && bytes[i+2] == 0x00 &&
        bytes[i+3] == 0x00 && bytes[i+4] == 0x80 && bytes[i+5] == 0x07 &&
        bytes[i+6] == 0xFF && bytes[i+7] == 0xFF && bytes[i+8] == 0x24 &&
        bytes[i+9] == 0x01) {
      patchLocation2 = i;
//      DBG("Found SSE3 Title address at 0x%08X\n",patchLocation2);
      break;
    }
    i++;
  }

  if (!patchLocation1 || !patchLocation2) {
//    DBG("Can't found SSE3 data addres or Title address at 0x%08X 0x%08X\n", patchLocation1, patchLocation2);
    return;
  }

//  DBG("Found SSE3 last data addres Start\n");
  i = patchLocation1 + 1500;
  //for (i=(patchLocation1+1500); i<(patchLocation1+3000); i++)
  while(true) {
    if (bytes[i] == 0x90 && bytes[i+1] == 0x90 && bytes[i+2] == 0x55 ) {
      patchlast = (i+1) - patchLocation1;
//      DBG("Found SSE3 last data addres at 0x%08X\n", patchlast);
      break;
    }
    i++;
  }

  if (!patchlast) {
 //   DBG("Can't found SSE3 data last addres at 0x%08X\n", patchlast);
    return;
  }
  // patch sse3_64 data

  for (i=0; i<patchlast; i++) {
    if (i<sizeof(sse3_patcher)) {
      bytes[patchLocation1 + i] = sse3_patcher[i];
    } else {
      bytes[patchLocation1 + i] = 0x90;
    }
  }

  // patch kHasSSE3 title
  bytes[patchLocation2 + 0] = 0xFC;
  bytes[patchLocation2 + 1] = 0x05;
  bytes[patchLocation2 + 8] = 0x2C;
  bytes[patchLocation2 + 9] = 0x00;

}

void Patcher_SSE3_5(void* kernelData)
{
  UINT8* bytes = (UINT8*)kernelData;
  UINT32 patchLocation1 = 0;
  UINT32 patchLocation2 = 0;
  UINT32 patchlast=0;
  UINT32 Length = sizeof(kernelData);
  UINT32 i;

//  DBG("Start find SSE3 address\n");

  for (i=256; i<(Length-256); i++) {
    if (bytes[i] == 0x66 && bytes[i+1] == 0x0F && bytes[i+2] == 0x6F &&
        bytes[i+3] == 0x44 && bytes[i+4] == 0x0E && bytes[i+5] == 0xF1 &&
        bytes[i-1680-32] == 0x55) {
      patchLocation1 = i-1680-32;
//      DBG("Found SSE3 data address at 0x%08X\n",patchLocation1);
    }

    // khasSSE2+..... title
    if (bytes[i] == 0xF3 && bytes[i+1] == 0x07 && bytes[i+2] == 0x00 &&
        bytes[i+3] == 0x00 && bytes[i+4] == 0x80 && bytes[i+5] == 0x07 &&
        bytes[i+6] == 0xFF && bytes[i+7] == 0xFF && bytes[i+8] == 0x24 &&
        bytes[i+9] == 0x01) {
      patchLocation2 = i;
//      DBG("Found SSE3 Title address at 0x%08X\n",patchLocation2);
      break;
    }
  }

  if (!patchLocation1 || !patchLocation2) {
//    DBG("Can't found SSE3 data addres or Title address at 0x%08X 0x%08X\n", patchLocation1, patchLocation2);
    return;
  }

//  DBG("Found SSE3 last data addres Start\n");

  for (i=(patchLocation1+1500);i<Length;i++) {
    if (bytes[i] == 0x90 && bytes[i+1] == 0x90 && bytes[i+2] == 0x55) {
      patchlast = (i+1) - patchLocation1;
//      DBG("Found SSE3 last data addres at 0x%08X\n", patchlast);
      break;
    }
  }

  if (!patchlast) {
//    DBG("Can't found SSE3 data last addres at 0x%08X\n", patchlast);
    return;
  }

  // patech sse3_64 data

  for (i=0; i<patchlast; i++) {
    if (i<sizeof(sse3_5_patcher)) {
      bytes[patchLocation1 + i] = sse3_5_patcher[i];
    } else {
      bytes[patchLocation1 + i] = 0x90;
    }
  }

  // patch kHasSSE3 title
  bytes[patchLocation2 + 0] = 0x0C;
  bytes[patchLocation2 + 1] = 0x06;
  bytes[patchLocation2 + 8] = 0x2C;
  bytes[patchLocation2 + 9] = 0x00;

}

void Patcher_SSE3_7()
{
     // not support yet
     return;
}
#endif

UINT32 LOADER_ENTRY::Get_Symtab(UINT8*  binary)
{
  UINT32  ncmds, cmdsize;
  UINT32  binaryIndex;
  UINTN   cnt;
//  UINT8*  binary = &KernelData[KernelOffset];
  struct  load_command        *loadCommand;
//  struct  symtab_command      *symCmd;
  
  if (MACH_GET_MAGIC(binary) != MH_MAGIC_64 ) {
    DBG("wrong binary at 0x%llX\n", (UINTN)binary);
    return 0;
  }
  
  ncmds = MACH_GET_NCMDS(binary);
  binaryIndex = sizeof(struct mach_header_64);
  DBG("ncmd = %d\n", ncmds);
  for (int j=0; j<20; ++j) {
    DBG("%02x", binary[j]);
  }
  DBG("\n");
  if (ncmds > 1000) ncmds = 0;
  
  for (cnt = 0; cnt < ncmds; cnt++) {
    loadCommand = (struct load_command *)(binary + binaryIndex);
    cmdsize = loadCommand->cmdsize;
    
    switch (loadCommand->cmd) {
    case LC_SYMTAB:
 //     *symCmd = binaryIndex;
      //      struct symtab_command {
      //        uint32_t  cmd;    /* LC_SYMTAB == 2 */
      //        uint32_t  cmdsize;  /* sizeof(struct symtab_command) */
      //        uint32_t  symoff;    /* symbol table offset */
      //        uint32_t  nsyms;    /* number of symbol table entries */
      //        uint32_t  stroff;    /* string table offset */
      //        uint32_t  strsize;  /* string table size in bytes */
      //      };
      //AddrVtable = symCmd->symoff;
      //SizeVtable = symCmd->nsyms;
      //NamesTable = symCmd->stroff;
      //DBG("SymTab: AddrVtable=0x%x SizeVtable=0x%x NamesTable=0x%x\n", AddrVtable, SizeVtable, NamesTable);
      return binaryIndex; //continue search for last command? or return here?
      
    default:
      break;

    }
    binaryIndex += cmdsize;
  }
  return 0;
}

void DumpSeg(struct  segment_command_64  *segCmd64)
{
  DBG("segCmd64->segname = %s\n",segCmd64->segname);
  DBG("segCmd64->vmaddr = 0x%08llX\n",segCmd64->vmaddr);
  DBG("segCmd64->vmsize = 0x%08llX\n",segCmd64->vmsize);
  
  DBG("fileoff = 0x%08llX\n",segCmd64->fileoff);
  DBG("filesize = 0x%08llX\n",segCmd64->filesize);
  //DBG("maxprot = 0x%08X\n",segCmd64->maxprot);
  //DBG("initprot = 0x%08X\n",segCmd64->initprot);
  //DBG("nsects = 0x%08X\n",segCmd64->nsects);
  DBG("flags = 0x%08X\n",segCmd64->flags);

}

UINT32 LOADER_ENTRY::GetTextExec()
{
  UINT32  ncmds, cmdsize;
  UINT32  binaryIndex;
  struct  segment_command_64  *segCmd64;
  
  ncmds = MACH_GET_NCMDS(KernelData);
  binaryIndex = sizeof(struct mach_header_64);
  
  for (UINTN cnt = 0; cnt < ncmds; cnt++) {
    segCmd64 = (struct segment_command_64 *)(KernelData + binaryIndex);
    cmdsize = segCmd64->cmdsize;
    
    switch (segCmd64->cmd) {
    case LC_SEGMENT_64: //19
      if (strcmp(segCmd64->segname, kTextExecSegment) == 0) {
        return (UINT32)(segCmd64->fileoff);
      }
      break;
    default:
      break;
    }
    binaryIndex += cmdsize;
  }
  
  return 0;
}

void LOADER_ENTRY::Get_PreLink()
{
  UINT32  ncmds, cmdsize;
  UINT32  binaryIndex;
  UINT8*  binary = &KernelData[0];
  struct  load_command        *loadCommand;
//  struct  segment_command     *segCmd;
  struct  segment_command_64  *segCmd64;
//  struct  symtab_command      *symCmd;
  UINTN kernelvmAddr = 0;


  if (is64BitKernel) {
    binaryIndex = sizeof(struct mach_header_64);
  } else {
    binaryIndex = sizeof(struct mach_header);
  }

  ncmds = MACH_GET_NCMDS(binary);

  for (UINTN cnt = 0; cnt < ncmds; cnt++) {
    loadCommand = (struct load_command *)(binary + binaryIndex);
    cmdsize = loadCommand->cmdsize;

    switch (loadCommand->cmd) {
      case LC_SEGMENT_64: //19
        segCmd64 = (struct segment_command_64 *)loadCommand;
        if (strcmp(segCmd64->segname, kTextSegment) == 0) {
          kernelvmAddr = segCmd64->vmaddr;
        }
        if (strcmp(segCmd64->segname, kPrelinkTextSegment) == 0) {
 //         DBG("Found PRELINK_TEXT, 64bit\n");
          if (segCmd64->vmsize > 0) {
            // 64bit segCmd64->vmaddr is 0xffffff80xxxxxxxx
            // PrelinkTextAddr = xxxxxxxx + KernelRelocBase
       //?     PrelinkTextAddr = (UINT32)(segCmd64->vmaddr ? (segCmd64->vmaddr - kernelvmAddr) + KernelRelocBase : 0);
            PrelinkTextAddr = (UINT32)(segCmd64->vmaddr ? segCmd64->vmaddr + KernelRelocBase : 0);
            PrelinkTextSize = (UINT32)(segCmd64->filesize);
            PrelinkTextLoadCmdAddr = binaryIndex; //(UINT32)(UINTN)segCmd64;
          }
#if KERNEL_DEBUG
          DumpSeg(segCmd64);
          DBG("PrelinkTextLoadCmdAddr = 0x%X, PrelinkTextAddr = 0x%X, PrelinkTextSize = 0x%X\n",
              PrelinkTextLoadCmdAddr, PrelinkTextAddr, PrelinkTextSize);
          UINT32 PrelinkTextAddr32 = (UINT32)(segCmd64->vmaddr - kernelvmAddr);
          for (int j=0; j<30; ++j) {
            DBG("%02x", binary[PrelinkTextAddr32+j]);
          }
          DBG("\n");
#endif
        }
        if (strcmp(segCmd64->segname, kPrelinkInfoSegment) == 0) {
          UINT32 sectionIndex;
          struct section_64 *sect;

          DumpSeg(segCmd64);
          sectionIndex = sizeof(struct segment_command_64);

          while(sectionIndex < segCmd64->cmdsize) {
            sect = (struct section_64 *)((UINT8*)segCmd64 + sectionIndex);

            if(strcmp(sect->sectname, kPrelinkInfoSection) == 0 && strcmp(sect->segname, kPrelinkInfoSegment) == 0) {
              if (sect->size > 0) {
                // 64bit sect->addr is 0xffffff80xxxxxxxx
                // PrelinkInfoAddr = xxxxxxxx + KernelRelocBase
                PrelinkInfoLoadCmdAddr = binaryIndex + sectionIndex; //(UINT32)(UINTN)sect;
                PrelinkInfoAddr = (UINT32)(sect->addr ? sect->addr + KernelRelocBase : 0);
                PrelinkInfoSize = (UINT32)sect->size;
              }
              DBG("__info found at 0x%llx: addr = 0x%x, size = 0x%llx\n", (UINTN)sect, sect->offset, sect->size);
              DBG("PrelinkInfoLoadCmdAddr = 0x%X, PrelinkInfoAddr = 0x%X, PrelinkInfoSize = 0x%X\n",
                  PrelinkInfoLoadCmdAddr, PrelinkInfoAddr, PrelinkInfoSize);
            }
            sectionIndex += sizeof(struct section_64);
          }
        }
        break;
#if 0  //exclude 32bit
      case LC_SEGMENT:
        segCmd = (struct segment_command *)loadCommand;
        //DBG("segCmd->segname = %s\n",segCmd->segname);
        //DBG("segCmd->vmaddr = 0x%08X\n",segCmd->vmaddr)
        //DBG("segCmd->vmsize = 0x%08X\n",segCmd->vmsize);
        if (strcmp(segCmd->segname, kPrelinkTextSegment) == 0) {
          //DBG("Found PRELINK_TEXT, 32bit\n");
          if (segCmd->vmsize > 0) {
            // PrelinkTextAddr = vmaddr + KernelRelocBase
            PrelinkTextAddr = (UINT32)(segCmd->vmaddr ? segCmd->vmaddr + KernelRelocBase : 0);
            PrelinkTextSize = (UINT32)segCmd->vmsize;
            PrelinkTextLoadCmdAddr = (UINT32)(UINTN)segCmd;
          }
          //DBG("at 0x%llx: vmaddr = 0x%x, vmsize = 0x%x\n", (UINTN)segCmd, segCmd->vmaddr, segCmd->vmsize);
          //DBG("PrelinkTextLoadCmdAddr = 0x%X, PrelinkTextAddr = 0x%X, PrelinkTextSize = 0x%X\n",
          //    PrelinkTextLoadCmdAddr, PrelinkTextAddr, PrelinkTextSize);
          //gBS->Stall(30*1000000);
        }
        if (strcmp(segCmd->segname, kPrelinkInfoSegment) == 0) {
          UINT32 sectionIndex;
          struct section *sect;

          //DBG("Found PRELINK_INFO, 32bit\n");
          //DBG("cmd = 0x%08X\n",segCmd->cmd);
          //DBG("cmdsize = 0x%08X\n",segCmd->cmdsize);
          //DBG("vmaddr = 0x%08X\n",segCmd->vmaddr);
          //DBG("vmsize = 0x%08X\n",segCmd->vmsize);
          //DBG("fileoff = 0x%08X\n",segCmd->fileoff);
          //DBG("filesize = 0x%08X\n",segCmd->filesize);
          //DBG("maxprot = 0x%08X\n",segCmd->maxprot);
          //DBG("initprot = 0x%08X\n",segCmd->initprot);
          //DBG("nsects = 0x%08X\n",segCmd->nsects);
          //DBG("flags = 0x%08X\n",segCmd->flags);
          sectionIndex = sizeof(struct segment_command);

          while(sectionIndex < segCmd->cmdsize) {
            sect = (struct section *)((UINT8*)segCmd + sectionIndex);
            sectionIndex += sizeof(struct section);

            if(strcmp(sect->sectname, kPrelinkInfoSection) == 0 && strcmp(sect->segname, kPrelinkInfoSegment) == 0) {
              if (sect->size > 0) {
                // PrelinkInfoAddr = sect->addr + KernelRelocBase
                PrelinkInfoLoadCmdAddr = (UINT32)(UINTN)sect;
                PrelinkInfoAddr = (UINT32)(sect->addr ? sect->addr + KernelRelocBase : 0);
                PrelinkInfoSize = (UINT32)sect->size;
              }
              //DBG("__info found at 0x%llx: addr = 0x%x, size = 0x%x\n", (UINTN)sect, sect->addr, sect->size);
              //DBG("PrelinkInfoLoadCmdAddr = 0x%X, PrelinkInfoAddr = 0x%X, PrelinkInfoSize = 0x%X\n",
              //    PrelinkInfoLoadCmdAddr, PrelinkInfoAddr, PrelinkInfoSize);
              //gBS->Stall(30*1000000);
            }
          }
        }
        break;

#endif
      default:
        break;
    }
    binaryIndex += cmdsize;
  }

  //gBS->Stall(20*1000000);
  return;
}

void
LOADER_ENTRY::FindBootArgs()
{
  UINT8           *ptr;
  UINT8           archMode = sizeof(UINTN) * 8;

  // start searching from 0x200000.
  ptr = (UINT8*)0x200000ull;


  while(true) {

    // check bootargs for 10.7 and up
    bootArgs2 = (BootArgs2*)ptr;

    if (bootArgs2->Version==2 && (bootArgs2->Revision==0 || bootArgs2->Revision==1)
        // plus additional checks - some values are not inited by boot.efi yet
        && bootArgs2->efiMode == archMode
        && bootArgs2->kaddr == 0 && bootArgs2->ksize == 0
        && bootArgs2->efiSystemTable == 0
        ) {
      // set vars
      dtRoot = (CHAR8*)(UINTN)bootArgs2->deviceTreeP;
      dtLength = &bootArgs2->deviceTreeLength;
      KernelSlide = bootArgs2->kslide;

      DBG_RT( "Found bootArgs2 at 0x%llX, DevTree at 0x%llX\n", (UINTN)ptr, (UINTN)bootArgs2->deviceTreeP);
      //DBG_RT("bootArgs2->kaddr = 0x%llX and bootArgs2->ksize =  0x%llX\n", bootArgs2->kaddr, bootArgs2->ksize);
      //DBG("bootArgs2->efiMode = 0x%02X\n", bootArgs2->efiMode);
      DBG_RT( "bootArgs2->CommandLine = %s\n", bootArgs2->CommandLine);
      DBG_RT( "bootArgs2->flags = 0x%hx\n", bootArgs2->flags);
      DBG_RT( "bootArgs2->kslide = 0x%x\n", bootArgs2->kslide);
      DBG_RT( "bootArgs2->bootMemStart = 0x%llx\n", bootArgs2->bootMemStart);
 //     if (KernelAndKextPatches && KernelAndKextPatches.KPDebug)
      Stall(5000000);

      // disable other pointer
      bootArgs1 = NULL;
      break;
    }

    // check bootargs for 10.4 - 10.6.x
    /*
    bootArgs1 = (BootArgs1*)ptr;

    if (bootArgs1->Version==1
        && (bootArgs1->Revision==6 || bootArgs1->Revision==5 || bootArgs1->Revision==4)
        // plus additional checks - some values are not inited by boot.efi yet
        && bootArgs1->efiMode == archMode
        && bootArgs1->kaddr == 0 && bootArgs1->ksize == 0
        && bootArgs1->efiSystemTable == 0
        ) {
      // set vars
      dtRoot = (CHAR8*)(UINTN)bootArgs1->deviceTreeP;
      dtLength = &bootArgs1->deviceTreeLength;

      DBG_RT( "Found bootArgs1 at 0x%8s, DevTree at %p\n", ptr, dtRoot);
      //DBG("bootArgs1->kaddr = 0x%08X and bootArgs1->ksize =  0x%08X\n", bootArgs1->kaddr, bootArgs1->ksize);
      //DBG("bootArgs1->efiMode = 0x%02X\n", bootArgs1->efiMode);

      // disable other pointer
      bootArgs2 = NULL;
      break;
    }
*/
    ptr += 0x1000;
    if ((UINTN)ptr > 0x70000000ull) {
      DBG_RT("bootArgs not found\n");
      bootArgs2 = 0;
      break;
    }
  }
}

XBool
LOADER_ENTRY::KernelUserPatch()
{
  INTN Num, y = 0;


  // old confuse
  // We are using KernelAndKextPatches as set by Custom Entries.
  // while config patches go to gSettings.KernelAndKextPatches
  // how to resolve it?
  
  for (size_t i = 0 ; i < KernelAndKextPatches.KernelPatches.size(); ++i) {
    DBG( "Patch[%zu]: %s\n", i, KernelAndKextPatches.KernelPatches[i].Label.c_str());
    if (!KernelAndKextPatches.KernelPatches[i].MenuItem.BValue) {
      //DBG_RT( "Patch[%d]: %a :: is not allowed for booted OS %a\n", i, KernelAndKextPatches.KernelPatches[i].Label, macOSVersion);
      DBG( "==> disabled\n");
      continue;
    }
    // if we modify directly KernelAndKextPatches.KernelPatches[i].SearchLen, it will wrong for next driver
    UINTN SearchLen = KernelAndKextPatches.KernelPatches[i].SearchLen;
    XBool once = false;
    UINTN procLen = 0;
    UINTN procAddr = searchProc(KernelAndKextPatches.KernelPatches[i].ProcedureName);
    DBG("procedure %s found at 0x%llx\n", KernelAndKextPatches.KernelPatches[i].ProcedureName.c_str(), procAddr);
    if (SearchLen == 0) {
      SearchLen = KERNEL_MAX_SIZE;
      procLen = KERNEL_MAX_SIZE - procAddr;
      once = true;
    } else {
      procLen = SearchLen;
    }
    UINT8 * curs = &KernelData[procAddr];
    UINTN j = 0;
    while (j < KERNEL_MAX_SIZE) {
      if (KernelAndKextPatches.KernelPatches[i].StartPattern.isEmpty() || //old behavior
          CompareMemMask((const UINT8*)curs,
                         KernelAndKextPatches.KernelPatches[i].StartPattern.data(),
                         KernelAndKextPatches.KernelPatches[i].StartPattern.size(),
                         KernelAndKextPatches.KernelPatches[i].StartMask.data(),
                         KernelAndKextPatches.KernelPatches[i].StartPattern.size())) {
        DBG( " StartPattern found\n");
        Num = SearchAndReplaceMask(curs,
                                   procLen,
                                   (const UINT8*)KernelAndKextPatches.KernelPatches[i].Find.data(),
                                   (const UINT8*)KernelAndKextPatches.KernelPatches[i].MaskFind.data(),
                                   KernelAndKextPatches.KernelPatches[i].Find.size(),
                                   (const UINT8*)KernelAndKextPatches.KernelPatches[i].Replace.data(),
                                   (const UINT8*)KernelAndKextPatches.KernelPatches[i].MaskReplace.data(),
                                   KernelAndKextPatches.KernelPatches[i].Count,
                                   KernelAndKextPatches.KernelPatches[i].Skip);
        
        if (Num) {
          y++;
          curs += SearchLen - 1;
          j    += SearchLen - 1;
        }
        DBG( "==> %s : %lld replaces done\n", Num ? "Success" : "Error", Num);
        if ( once || KernelAndKextPatches.KernelPatches[i].StartPattern.isEmpty() ) {
          break;
        }
      }
      j++; curs++;
    }
  }
  if (KernelAndKextPatches.KPDebug) {
    gBS->Stall(2000000);
  }

  return (y != 0);
}

XBool
LOADER_ENTRY::BooterPatch(IN UINT8 *BooterData, IN UINT64 BooterSize)
{
  INTN Num, y = 0;


  for (size_t i = 0 ; i < KernelAndKextPatches.BootPatches.size(); ++i)
  {
    // if we modify directly KernelAndKextPatches.BootPatches[i].SearchLen, it will wrong for next driver
    UINTN SearchLen = KernelAndKextPatches.BootPatches[i].SearchLen;
    if (!SearchLen) {
      SearchLen = BooterSize;
    }

    DBG( "Patch[%zu]: %s\n", i, KernelAndKextPatches.BootPatches[i].Label.c_str());
    if (!KernelAndKextPatches.BootPatches[i].MenuItem.BValue) {
      DBG( "==> disabled\n");
      continue;
    }
    UINT8 * curs = BooterData;
    UINTN j = 0;
    while (j < BooterSize) {
      if (KernelAndKextPatches.BootPatches[i].StartPattern.isEmpty() || //old behavior
          CompareMemMask((const UINT8*)curs,
                         (const UINT8*)KernelAndKextPatches.BootPatches[i].StartPattern.data(),
                         KernelAndKextPatches.BootPatches[i].StartPattern.size(),
                         (const UINT8*)KernelAndKextPatches.BootPatches[i].StartMask.data(),
                         KernelAndKextPatches.BootPatches[i].StartPattern.size())) {
        DBG( " StartPattern found\n");

        Num = SearchAndReplaceMask(curs,
                                   SearchLen,
                                   (const UINT8*)KernelAndKextPatches.BootPatches[i].Find.data(),
                                   (const UINT8*)KernelAndKextPatches.BootPatches[i].MaskFind.data(),
                                   KernelAndKextPatches.BootPatches[i].Find.size(),
                                   (const UINT8*)KernelAndKextPatches.BootPatches[i].Replace.data(),
                                   (const UINT8*)KernelAndKextPatches.BootPatches[i].MaskReplace.data(),
                                   KernelAndKextPatches.BootPatches[i].Count,
                                   KernelAndKextPatches.BootPatches[i].Skip);
        if (Num) {
          y++;
          curs += SearchLen - 1;
          j    += SearchLen - 1;
        }

        DBG( "==> %s : %lld replaces done\n", Num ? "Success" : "Error", Num);
        if ( KernelAndKextPatches.BootPatches[i].StartPattern.isEmpty() ) {
          break;
        }
      }
      j++; curs++;
    }
  }
  if (KernelAndKextPatches.KPDebug) {
    gBS->Stall(2000000);
  }
  return (y != 0);
}

//void
//LOADER_ENTRY::KernelAndKextPatcherInit()
//{
//  if (PatcherInited) {
//    DBG("patcher inited\n");
//    return;
//  }
//
//  PatcherInited = true;
//
//  // KernelRelocBase will normally be 0
//  // but if OsxAptioFixDrv is used, then it will be > 0
//  SetKernelRelocBase();
//	DBG("KernelRelocBase = %llx\n", KernelRelocBase);
//
//  // Find bootArgs - we need then for proper detection
//  // of kernel Mach-O header
//  FindBootArgs();
//  if (bootArgs1 == NULL && bootArgs2 == NULL) {
//    DBG_RT("BootArgs not found - skipping patches!\n");
//    return;
//  }
//
//  // Find kernel Mach-O header:
//  // for 10.4 - 10.5: 0x00111000
//  // for 10.6 - 10.7: 0x00200000
//  // for ML: bootArgs2->kslide + 0x00200000
//  // for AptioFix booting - it's always at KernelRelocBase + 0x00200000
//
////  UINT64 os_version = AsciiOSVersionToUint64(macOSVersion);
//    DBG("macOSVersion=%s\n", macOSVersion.asString().c_str());
//
////  if (macOSVersion.notEmpty() && macOSVersion < AsciiOSVersionToUint64("10.6")) {
////    KernelData = (UINT8*)(UINTN)(KernelSlide + KernelRelocBase + 0x00111000);
////  } else {
//    KernelData = (UINT8*)(UINTN)(KernelSlide + KernelRelocBase + 0x00200000);
////  }
//
//  // check that it is Mach-O header and detect architecture
//  if(MACH_GET_MAGIC(KernelData) == MH_MAGIC || MACH_GET_MAGIC(KernelData) == MH_CIGAM) {
//    DBG("Found 32 bit kernel at 0x%llx\n", (UINTN)KernelData);
//    is64BitKernel = false;
//  } else if (MACH_GET_MAGIC(KernelData) == MH_MAGIC_64 || MACH_GET_MAGIC(KernelData) == MH_CIGAM_64) {
//    DBG( "Found 64 bit kernel at 0x%llx\n", (UINTN)KernelData);
////    DBG_RT("text section is: %s\n", (const char*)&KernelData[0x28]);
///*
//    KernelOffset = 0;
//    while (KernelOffset < KERNEL_MAX_SIZE) {
//      if ((MACH_GET_MAGIC(KernelData+KernelOffset) == MH_MAGIC_64 ) || (MACH_GET_MAGIC(KernelData+KernelOffset) == MH_CIGAM_64)) {
//        DBG("dump at offset 0x%x\n", KernelOffset);
//        for (int j = 0; j<20; ++j) {
//          DBG("%02x ", KernelData[KernelOffset+j]);
//        }
//        DBG("\n");
//        if ((((struct mach_header_64*)(KernelData+KernelOffset))->filetype) == MH_EXECUTE) {
//          DBG("execute found\n");
//          break;
//        }
//      }
//      KernelOffset += 4;
//    }
// */
//    if ((((struct mach_header_64*)KernelData)->filetype) == MH_KERNEL_COLLECTION) {
//      // BigSur
//      KernelOffset = GetTextExec();
////      DBG("BigSur: KernelOffset =0x%X\n", KernelOffset);
//    }
//    is64BitKernel = true;
//  } else {
//    // not valid Mach-O header - exiting
//    DBG( "Kernel not found at 0x%llx - skipping patches!\n", (UINTN)KernelData);
//    KernelData = NULL;
//    return;
//  }
//  DBG( " kernel offset at 0x%x\n", KernelOffset);
//  // find __PRELINK_TEXT and __PRELINK_INFO
//  if ((((struct mach_header_64*)KernelData)->filetype) == MH_KERNEL_COLLECTION) {
//    Get_PreLink(); // BigSur
//  } else {
//    Get_PreLink();
//  }
//  //find symbol tables
//  struct  symtab_command  *symCmd = NULL;
//  UINT32 symCmdOffset = Get_Symtab(&KernelData[KernelOffset]);
//  if (symCmdOffset != 0) {
//    symCmd = (struct symtab_command *)&KernelData[KernelOffset + symCmdOffset];
//    AddrVtable = symCmd->symoff; //this offset relative to KernelData+0
//    SizeVtable = symCmd->nsyms;
//    NamesTable = symCmd->stroff;
//    DBG("Kernel: AddrVtable=0x%x SizeVtable=0x%x NamesTable=0x%x\n", AddrVtable, SizeVtable, NamesTable);
//  }
///*
//  for (UINTN i=0x00200000; i<0x30000000; i+=4) {
//    UINT32 *KD = (UINT32 *)i;
//    if ((KD[0] == MH_MAGIC_64) && (KD[0x0a] == 0x45545F5F)){
//      DBG_RT( "Found MAGIC at %llx, text=%s\n", i, (const char*)&KD[0x0a]);
//      DBG( "Found MAGIC at %llx, text=%s\n", i, (const char*)&KD[0x0a]);
//      KernelData = (UINT8*)KD;
//      DBG( "Found new kernel at 0x%llx\n", (UINTN)KernelData);
//      break;
//    }
//  }
//*/
//  if (EFI_ERROR(getVTable())) {
//    DBG("error getting vtable: \n");
//  }
//
//  isKernelcache = (PrelinkTextSize > 0) && (PrelinkInfoSize > 0);
//	DBG( "isKernelcache: %ls\n", isKernelcache ? L"Yes" : L"No");
//}
//
//void
//LOADER_ENTRY::KernelAndKextsPatcherStart()
//{
//  XBool KextPatchesNeeded, patchedOk;
//  /*
//   * it was intended for custom entries but not work if no custom entries used
//   * so set common until better solution invented
//   */
//  //KernelAndKextPatches = (KERNEL_AND_KEXT_PATCHES *)(((UINTN)&gSettings) + OFFSET_OF(SETTINGS_DATA, KernelAndKextPatches));
////  CopyKernelAndKextPatches(&KernelAndKextPatches, &gSettings.KernelAndKextPatches);
//  KernelAndKextPatches = gSettings.KernelAndKextPatches;
//
//  PatcherInited = false;
//  KernelAndKextPatcherInit();
//
//  KextPatchesNeeded = (
//    KernelAndKextPatches.KPAppleIntelCPUPM || GlobalConfig.NeedPMfix ||
//    KernelAndKextPatches.KPAppleRTC ||
//    KernelAndKextPatches.EightApple ||
//    KernelAndKextPatches.KPDELLSMBIOS ||
//    KernelAndKextPatches.KPATIConnectorsPatch.notEmpty() ||
//    KernelAndKextPatches.KextPatches.size() > 0
//  );
//
////  DBG_RT("\nKernelToPatch: ");
////  DBG_RT("Kernels patches: %d\n", KernelAndKextPatches.KernelPatches.size());
//  if (GlobalConfig.KernelPatchesAllowed && KernelAndKextPatches.KernelPatches.notEmpty()) {
////    DBG_RT("Enabled: \n");
//    DBG("Kernels patches: enabled \n");
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//    if (EFI_ERROR(getVTable())) {
////      DBG_RT("error getting vtable: \n");
//      goto NoKernelData;
//    }
//    patchedOk = KernelUserPatch();
////    DBG_RT(patchedOk ? " OK\n" : " FAILED!\n");
////    gBS->Stall(5000000);
//  } else {
////    DBG_RT("Disabled\n");
//  }
///*
//  DBG_RT( "\nKernelCpu patch: ");
//  if (KernelAndKextPatches.KPKernelCpu) {
//    //
//    // Kernel patches
//    //
//    DBG_RT( "Enabled: \n");
//    KernelAndKextPatcherInit();
//    if (KernelData == NULL) goto NoKernelData;
//    if(is64BitKernel) {
//      DBG_RT( "64 bit patch ...\n");
//      KernelPatcher_64();
//    } else {
//      DBG_RT( "32 bit patch ...\n");
//      KernelPatcher_32();
//    }
//    DBG_RT( " OK\n");
//  } else {
//    DBG_RT( "Disabled\n");
//  }
//*/
//  //other method for KernelCPU patch is FakeCPUID
//  DBG_RT( "\nFakeCPUID patch: ");
//  if (KernelAndKextPatches.FakeCPUID) {
//    DBG_RT( "Enabled: 0x%06x\n", KernelAndKextPatches.FakeCPUID);
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//    KernelCPUIDPatch();
//  } else {
//    DBG_RT( "Disabled\n");
//  }
//
//  // CPU power management patch for CPU with locked msr
//  DBG_RT( "\nKernelPm patch: ");
//  if (KernelAndKextPatches.KPKernelPm||GlobalConfig.NeedPMfix || KernelAndKextPatches.KPKernelXCPM) {
//    DBG_RT( "Enabled: \n");
//    DBG( "KernelPm patch: Enabled\n");
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//    patchedOk = false;
//    if (is64BitKernel) {
//      patchedOk = KernelPatchPm();
//    }
//    DBG_RT( patchedOk ? " OK\n" : " FAILED!\n");
//  } else {
//    DBG_RT( "Disabled\n");
//  }
//
//  // Patch to not dump kext at panic (c)vit9696
//  DBG_RT( "\nPanicNoKextDump patch: ");
//  if (KernelAndKextPatches.KPPanicNoKextDump) {
//    DBG_RT( "Enabled: \n");
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//    patchedOk = KernelPanicNoKextDump();
//    DBG_RT( patchedOk ? " OK\n" : " FAILED!\n");
//  } else {
//    DBG_RT( "Disabled\n");
//  }
//
//
//  // Lapic Panic Kernel Patch
//  DBG_RT( "\nKernelLapic patch: ");
//  if (KernelAndKextPatches.KPKernelLapic) {
//    DBG_RT( "Enabled: \n");
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//    if(is64BitKernel) {
//      DBG_RT( "64-bit patch ...\n");
//      patchedOk = KernelLapicPatch_64();
//    } else {
//      DBG_RT( "32-bit patch ...\n");
//      patchedOk = KernelLapicPatch_32();
//    }
//    DBG_RT( patchedOk ? " OK\n" : " FAILED!\n");
//  } else {
//    DBG_RT( "Disabled\n");
//  }
//
//  if (KernelAndKextPatches.KPKernelXCPM) {
//    //
//    // syscl - EnableExtCpuXCPM: Enable unsupported CPU's PowerManagement
//    //
////    EnableExtCpuXCPM = NULL;
//    patchedOk = false;
////    XBool apply_idle_patch = (gCPUStructure.Model >= CPU_MODEL_SKYLAKE_U) && gSettings.HWP;
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//
//    // syscl - now enable extra Cpu's PowerManagement
//    // only Intel support this feature till now
//    // move below code outside the if condition if AMD supports
//    // XCPM later on
//
//    if (gCPUStructure.Vendor == CPU_VENDOR_INTEL) {
//      switch (gCPUStructure.Model) {
//          case CPU_MODEL_JAKETOWN:
//            // SandyBridge-E LGA2011
//            patchedOk = SandyBridgeEPM();
//            gSNBEAICPUFixRequire = true;       // turn on SandyBridge-E AppleIntelCPUPowerManagement Fix
//            break;
//
//          case CPU_MODEL_IVY_BRIDGE:
//            // IvyBridge
//            patchedOk = KernelIvyBridgeXCPM();
//            break;
//
//          case CPU_MODEL_IVY_BRIDGE_E5:
//            // IvyBridge-E
//            patchedOk = KernelIvyE5XCPM();
//            break;
//
//          case CPU_MODEL_HASWELL_E:
//            // Haswell-E
//            patchedOk = HaswellEXCPM();
//            break;
//
//          case CPU_MODEL_BROADWELL_E5:
//          case CPU_MODEL_BROADWELL_DE:
//            // Broadwell-E/EP
//            patchedOk = BroadwellEPM();
//            gBDWEIOPCIFixRequire = true;
//            break;
//
//          default:
//            if (gCPUStructure.Model >= CPU_MODEL_HASWELL &&
//               ( gCPUStructure.BrandString.contains("Celeron") ||
//                 gCPUStructure.BrandString.contains("Pentium"))) {
//              // Haswell+ low-end CPU
//              patchedOk = HaswellLowEndXCPM();
//            }
//            break;
//      }
//    }
//	  DBG_RT( "EnableExtCpuXCPM - %s!\n", patchedOk? "OK" : "FAILED");
//  }
//
//  Stall(2000000);
//  //
//  // Kext patches
//  //
//
//  // we need to scan kexts if "InjectKexts true and CheckFakeSMC"
//  if (/*OSFLAG_ISSET(Flags, OSFLAG_WITHKEXTS) || */
//      OSFLAG_ISSET(Flags, OSFLAG_CHECKFAKESMC)) {
//    DBG_RT( "\nAllowing kext patching to check if FakeSMC is present\n");
//    GlobalConfig.KextPatchesAllowed = true;
//    KextPatchesNeeded = true;
//  }
//
//  DBG_RT( "\nKextPatches Needed: %c, Allowed: %c ... ",
//         (KextPatchesNeeded ? L'Y' : L'n'),
//         (GlobalConfig.KextPatchesAllowed ? L'Y' : L'n')
//         );
//
//  if (KextPatchesNeeded && GlobalConfig.KextPatchesAllowed) {
////    DBG_RT( "\nKext patching INIT\n");
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//    DBG_RT( "\nKext patching STARTED\n");
//    KextPatcherStart();  //is FakeSMC found in cache then inject will be disabled
//    DBG_RT( "\nKext patching ENDED\n");
//  } else {
//    DBG_RT( "Disabled\n");
//  }
//
//  Stall(1000000);
//
//  //
//  // Kext add
//  //
////  if (KernelAndKextPatches.KPDebug) {
////    if (OSFLAG_ISSET(Entry->Flags, OSFLAG_CHECKFAKESMC) &&
////        OSFLAG_ISUNSET(Entry->Flags, OSFLAG_WITHKEXTS)) {
////    disabled kext injection if FakeSMC is already present
////    Entry->Flags = OSFLAG_UNSET(Entry->Flags, OSFLAG_WITHKEXTS); //Slice - we are already here
////
////      DBG_RT( "\nInjectKexts: disabled because FakeSMC is already present and InjectKexts option set to Detect\n");
////      gBS->Stall(500000);
////    }
////  }
//
//  if (OSFLAG_ISSET(Flags, OSFLAG_WITHKEXTS)) {
//    UINT32      deviceTreeP;
//    UINT32      *deviceTreeLength;
//    EFI_STATUS  Status;
//    UINTN       DataSize;
//
//    // check if FSInject already injected kexts
//    DataSize = 0;
//    Status = gRT->GetVariable (L"FSInject.KextsInjected", &gEfiGlobalVariableGuid, NULL, &DataSize, NULL);
//    if (Status == EFI_BUFFER_TOO_SMALL) {
//      // var exists - just exit
//
//      DBG_RT( "\nInjectKexts: skipping, FSInject already injected them\n");
//      Stall(500000);
//      return;
//    }
//
////    KernelAndKextPatcherInit();
////    if (KernelData == NULL) goto NoKernelData;
//    if (bootArgs1 != NULL) {
//      deviceTreeP = bootArgs1->deviceTreeP;
//      deviceTreeLength = &bootArgs1->deviceTreeLength;
//    } else if (bootArgs2 != NULL) {
//      deviceTreeP = bootArgs2->deviceTreeP;
//      deviceTreeLength = &bootArgs2->deviceTreeLength;
//    } else return;
//
//    Status = InjectKexts(deviceTreeP, deviceTreeLength);
//    DBG_RT("Inject kexts done at 0x%llx\n", (UINTN)deviceTreeP);
//    if (!EFI_ERROR(Status)) KernelBooterExtensionsPatch();
//  }
//
//  return;
//
//NoKernelData:
//  Stall(5000000);
//}
