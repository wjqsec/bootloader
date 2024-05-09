/*
 *
 * Copyright (c) 2020 Jief
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

#ifndef __menu_items_H__
#define __menu_items_H__

#include <Efi.h>
#include "../../Platform/KERNEL_AND_KEXT_PATCHES.h"
#include "../../Platform/plist/plist.h"
#include "../../libeg/libeg.h"
#include "../../libeg/XIcon.h"
#include "../../refit/lib.h"
#include <UefiLoader.h> // for cpu_type_t
#include "../../Platform/boot.h"
#include "../../Platform/Volumes.h"

#include "../../cpp_foundation/XObjArray.h"
#include "../../cpp_foundation/XStringArray.h"
#include "../../cpp_foundation/XString.h"
#include "../../libeg/XPointer.h"
#include "../../Platform/MacOsVersion.h"


//
//#define REFIT_DEBUG (2)
//#define Print if ((!GlobalConfig.Quiet) || (gSettings.GUI.TextOnly)) Print
////#include "GenericBdsLib.h"


//#define TAG_ABOUT_OLD              (1)
//#define TAG_RESET_OLD              (2)
//#define TAG_SHUTDOWN_OLD           (3)
//#define TAG_TOOL_OLD               (4)
////#define TAG_LOADER             (5)
////#define TAG_LEGACY             (6)
//#define TAG_INFO_OLD               (7)
//#define TAG_OPTIONS            (8)
//#define TAG_INPUT_OLD              (9)
//#define TAG_HELP_OLD               (10) // wasn't used ?
//#define TAG_SWITCH_OLD             (11)
//#define TAG_CHECKBIT_OLD           (12)
//#define TAG_SECURE_BOOT_OLD        (13)
//#define TAG_SECURE_BOOT_CONFIG_OLD (14)
//#define TAG_CLOVER_OLD             (100)
//#define TAG_EXIT_OLD               (101)
//#define TAG_RETURN_OLD             ((UINTN)(-1))

//typedef struct _refit_menu_screen REFIT_MENU_SCREEN;
class REFIT_MENU_SCREEN;
class REFIT_MENU_SWITCH;
class REFIT_MENU_CHECKBIT;
class REFIT_MENU_ENTRY_CLOVER;
class REFIT_MENU_ITEM_RETURN;
class REFIT_INPUT_DIALOG;
class REFIT_INFO_DIALOG;
class REFIT_MENU_ENTRY_LOADER_TOOL;
class REFIT_MENU_ITEM_SHUTDOWN;
class REFIT_MENU_ITEM_RESET;
class REFIT_MENU_ITEM_ABOUT;
class REFIT_MENU_ITEM_OPTIONS;
class REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER;
class LOADER_ENTRY;
class LEGACY_ENTRY;
class REFIT_MENU_ENTRY_OTHER;
class REFIT_SIMPLE_MENU_ENTRY_TAG;
class REFIT_MENU_ENTRY_ITEM_ABSTRACT;
class REFIT_MENU_ITEM_BOOTNUM;
class XPointer;
class SIDELOAD_KEXT;

/**********************************************************  REFIT_ABSTRACT_MENU_ENTRY  *************************************************************/

class REFIT_ABSTRACT_MENU_ENTRY
{
  public:
  XStringW           Title = XStringW();
  XBool              Hidden = false;
  UINTN              Row = 0;
  wchar_t             ShortcutDigit = 0;
  wchar_t            ShortcutLetter = 0;
  XIcon              Image = 0;
  EG_RECT            Place = EG_RECT();
  ACTION             AtClick = ActionNone;
  ACTION             AtDoubleClick = ActionNone;
  ACTION             AtRightClick = ActionNone;
  ACTION             AtMouseOver = ActionNone;
  REFIT_MENU_SCREEN* SubScreen = NULL; // we can't use apd<REFIT_MENU_SCREEN*> because of circular reference. We would need to include REFIT_MENU_SCREEN.h, but REFIT_MENU_SCREEN.h includes menu_items.h

  virtual XIcon* getDriveImage() { return nullptr; };
  virtual XIcon* getBadgeImage() { return nullptr; };

  virtual REFIT_SIMPLE_MENU_ENTRY_TAG* getREFIT_SIMPLE_MENU_ENTRY_TAG() { return nullptr; };
  virtual REFIT_MENU_SWITCH* getREFIT_MENU_SWITCH() { return nullptr; };
  virtual REFIT_MENU_CHECKBIT* getREFIT_MENU_CHECKBIT() { return nullptr; };
  virtual REFIT_MENU_ENTRY_CLOVER* getREFIT_MENU_ENTRY_CLOVER() { return nullptr; };
  virtual REFIT_MENU_ITEM_RETURN* getREFIT_MENU_ITEM_RETURN() { return nullptr; };
  virtual REFIT_INPUT_DIALOG* getREFIT_INPUT_DIALOG() { return nullptr; };
  virtual REFIT_INFO_DIALOG* getREFIT_INFO_DIALOG() { return nullptr; };
  virtual REFIT_MENU_ENTRY_LOADER_TOOL* getREFIT_MENU_ENTRY_LOADER_TOOL() { return nullptr; };
  virtual REFIT_MENU_ITEM_SHUTDOWN* getREFIT_MENU_ITEM_SHUTDOWN() { return nullptr; };
  virtual REFIT_MENU_ITEM_RESET* getREFIT_MENU_ITEM_RESET() { return nullptr; };
  virtual REFIT_MENU_ITEM_ABOUT* getREFIT_MENU_ITEM_ABOUT() { return nullptr; };
  virtual REFIT_MENU_ITEM_OPTIONS* getREFIT_MENU_ITEM_OPTIONS() { return nullptr; };
  virtual REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER* getREFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER() { return nullptr; };
  virtual LOADER_ENTRY* getLOADER_ENTRY() { return nullptr; };
  virtual LEGACY_ENTRY* getLEGACY_ENTRY() { return nullptr; };
  virtual REFIT_MENU_ENTRY_OTHER* getREFIT_MENU_ENTRY_OTHER() { return nullptr; };
  virtual REFIT_MENU_ENTRY_ITEM_ABSTRACT* getREFIT_MENU_ITEM_IEM_ABSTRACT() { return nullptr; };
  virtual REFIT_MENU_ITEM_BOOTNUM* getREFIT_MENU_ITEM_BOOTNUM() { return nullptr; };
  virtual void StartLoader() {};
  virtual void StartLegacy() {};
  virtual void StartTool() {};

  REFIT_ABSTRACT_MENU_ENTRY() {};
  REFIT_ABSTRACT_MENU_ENTRY(const XStringW& Title_) : Title(Title_) {};
  REFIT_ABSTRACT_MENU_ENTRY(const XStringW& Title_, ACTION AtClick_) : Title(Title_), AtClick(AtClick_) {};
  REFIT_ABSTRACT_MENU_ENTRY(const XStringW& Title_, UINTN Row_, wchar_t ShortcutDigit_, wchar_t ShortcutLetter_, ACTION AtClick_) : Title(Title_), Row(Row_), ShortcutDigit(ShortcutDigit_), ShortcutLetter(ShortcutLetter_), AtClick(AtClick_) {};

  REFIT_ABSTRACT_MENU_ENTRY(const REFIT_ABSTRACT_MENU_ENTRY&) { panic("not yet defined"); }
  REFIT_ABSTRACT_MENU_ENTRY& operator=(const REFIT_ABSTRACT_MENU_ENTRY&) { panic("not yet defined"); }

  virtual ~REFIT_ABSTRACT_MENU_ENTRY(); // virtual destructor : this is vital
};



/**********************************************************  REFIT_ABSTRACT_MENU_ENTRY  *************************************************************/

	class REFIT_SIMPLE_MENU_ENTRY_TAG : public REFIT_ABSTRACT_MENU_ENTRY
	{
	public:
	  UINTN              Tag;

	  REFIT_SIMPLE_MENU_ENTRY_TAG(const XStringW& Title_, UINTN Tag_, ACTION AtClick_)
				 : REFIT_ABSTRACT_MENU_ENTRY(Title_, AtClick_), Tag(Tag_)
				 {};

	  virtual REFIT_SIMPLE_MENU_ENTRY_TAG* getREFIT_SIMPLE_MENU_ENTRY_TAG() { return this; };
	};



/**********************************************************  Simple entries. Inherit from REFIT_ABSTRACT_MENU_ENTRY  *************************************************************/

	class REFIT_MENU_ITEM_RETURN : public REFIT_ABSTRACT_MENU_ENTRY
	{
	public:
	  REFIT_MENU_ITEM_RETURN() : REFIT_ABSTRACT_MENU_ENTRY() {};
	  REFIT_MENU_ITEM_RETURN(const XStringW& Title_, UINTN Row_, wchar_t ShortcutDigit_, wchar_t ShortcutLetter_, ACTION AtClick_)
				 : REFIT_ABSTRACT_MENU_ENTRY(Title_, Row_, ShortcutDigit_, ShortcutLetter_, AtClick_)
				 {};
	  virtual REFIT_MENU_ITEM_RETURN* getREFIT_MENU_ITEM_RETURN() { return this; };
	};

	class REFIT_MENU_ITEM_SHUTDOWN : public REFIT_ABSTRACT_MENU_ENTRY
	{
	public:
	  REFIT_MENU_ITEM_SHUTDOWN() : REFIT_ABSTRACT_MENU_ENTRY() {};
	  REFIT_MENU_ITEM_SHUTDOWN(const XStringW& Title_, UINTN Row_, wchar_t ShortcutDigit_, wchar_t ShortcutLetter_, ACTION AtClick_)
				 : REFIT_ABSTRACT_MENU_ENTRY(Title_, Row_, ShortcutDigit_, ShortcutLetter_, AtClick_)
				 {};
	  virtual REFIT_MENU_ITEM_SHUTDOWN* getREFIT_MENU_ITEM_SHUTDOWN() { return this; };
	};

	class REFIT_MENU_ITEM_RESET : public REFIT_ABSTRACT_MENU_ENTRY {
	public:
	  REFIT_MENU_ITEM_RESET() : REFIT_ABSTRACT_MENU_ENTRY() {};
	  REFIT_MENU_ITEM_RESET(const XStringW& Title_, UINTN Row_, wchar_t ShortcutDigit_, wchar_t ShortcutLetter_, ACTION AtClick_)
				 : REFIT_ABSTRACT_MENU_ENTRY(Title_, Row_, ShortcutDigit_, ShortcutLetter_, AtClick_)
				 {};
	  virtual REFIT_MENU_ITEM_RESET* getREFIT_MENU_ITEM_RESET() { return this; };
	};

	class REFIT_MENU_ITEM_ABOUT : public REFIT_ABSTRACT_MENU_ENTRY
	{
	public:
	  REFIT_MENU_ITEM_ABOUT() : REFIT_ABSTRACT_MENU_ENTRY() {};
	  REFIT_MENU_ITEM_ABOUT(const XStringW& Title_, UINTN Row_, wchar_t ShortcutDigit_, wchar_t ShortcutLetter_, ACTION AtClick_)
				 : REFIT_ABSTRACT_MENU_ENTRY(Title_, Row_, ShortcutDigit_, ShortcutLetter_, AtClick_)
				 {};
	  virtual REFIT_MENU_ITEM_ABOUT* getREFIT_MENU_ITEM_ABOUT() { return this; };
	};

	class REFIT_MENU_ITEM_OPTIONS : public REFIT_ABSTRACT_MENU_ENTRY {
	public:
	  REFIT_MENU_ITEM_OPTIONS() : REFIT_ABSTRACT_MENU_ENTRY() {};
	  REFIT_MENU_ITEM_OPTIONS(const XStringW& Title_, UINTN Row_, wchar_t ShortcutDigit_, wchar_t ShortcutLetter_, ACTION AtClick_)
				 : REFIT_ABSTRACT_MENU_ENTRY(Title_, Row_, ShortcutDigit_, ShortcutLetter_, AtClick_)
				 {};
	  virtual REFIT_MENU_ITEM_OPTIONS* getREFIT_MENU_ITEM_OPTIONS() { return this; };
	};


	class REFIT_INFO_DIALOG : public REFIT_ABSTRACT_MENU_ENTRY {
	public:
	  virtual REFIT_INFO_DIALOG* getREFIT_INFO_DIALOG() { return this; };
	};



/**********************************************************    *************************************************************/

	class REFIT_MENU_ENTRY_ITEM_ABSTRACT : public REFIT_ABSTRACT_MENU_ENTRY {
	public:
	  INPUT_ITEM        *Item;
	  REFIT_MENU_ENTRY_ITEM_ABSTRACT() : Item(0) {}
    REFIT_MENU_ENTRY_ITEM_ABSTRACT(const REFIT_MENU_ENTRY_ITEM_ABSTRACT&) = delete;
    REFIT_MENU_ENTRY_ITEM_ABSTRACT& operator=(const REFIT_MENU_ENTRY_ITEM_ABSTRACT&) = delete;
	  virtual REFIT_MENU_ENTRY_ITEM_ABSTRACT* getREFIT_MENU_ITEM_IEM_ABSTRACT() { return this; };
	};

		/* Classes that needs a Item member */
		class REFIT_INPUT_DIALOG : public REFIT_MENU_ENTRY_ITEM_ABSTRACT {
		public:
		  virtual REFIT_INPUT_DIALOG* getREFIT_INPUT_DIALOG() { return this; };
		};

		class REFIT_MENU_SWITCH : public REFIT_MENU_ENTRY_ITEM_ABSTRACT {
		public:
		  virtual REFIT_MENU_SWITCH* getREFIT_MENU_SWITCH() { return this; };
		};

		class REFIT_MENU_CHECKBIT : public REFIT_MENU_ENTRY_ITEM_ABSTRACT {
		public:
		  virtual REFIT_MENU_CHECKBIT* getREFIT_MENU_CHECKBIT() { return this; };
		};

/**********************************************************  Loader entries  *************************************************************/
	/*
	 * Super class of LOADER_ENTRY & LEGACY_ENTRY
	 */
	class REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER : public REFIT_ABSTRACT_MENU_ENTRY
	{
	public:
	  XStringW     DevicePathString;
	  XString8Array LoadOptions; //moved here for compatibility with legacy
	  XStringW     LoaderPath;
    XIcon        DriveImage;
    XIcon        BadgeImage;

    REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER()
        : REFIT_ABSTRACT_MENU_ENTRY(), DevicePathString(), LoadOptions(), LoaderPath(), DriveImage(), BadgeImage()
        {}
    REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER(const REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER&) = delete;
    REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER& operator=(const REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER&) = delete;

    virtual  XIcon* getDriveImage()  { return &DriveImage; };
    virtual  XIcon* getBadgeImage()  { return &BadgeImage; };

	  virtual REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER* getREFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER() { return this; };
	};

		//---------------------------------------  REFIT_MENU_ENTRY_LOADER_TOOL  ---------------------------------------//

		class REFIT_MENU_ENTRY_LOADER_TOOL : public REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER
		{
		  public:
			UINT8             NoMemset; //HACK - some non zero value
			UINT16            Flags;
			apd<EFI_DEVICE_PATH*> DevicePath;
      
      void              StartTool();

			REFIT_MENU_ENTRY_LOADER_TOOL() : REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER(), NoMemset(1), Flags(0), DevicePath(0) {};
      REFIT_MENU_ENTRY_LOADER_TOOL(const REFIT_MENU_ENTRY_LOADER_TOOL&) = delete;
      REFIT_MENU_ENTRY_LOADER_TOOL& operator=(const REFIT_MENU_ENTRY_LOADER_TOOL&) = delete;

			virtual REFIT_MENU_ENTRY_LOADER_TOOL* getREFIT_MENU_ENTRY_LOADER_TOOL() { return this; };
		};


		//---------------------------------------  REFIT_MENU_ENTRY_CLOVER  ---------------------------------------//

		class REFIT_MENU_ENTRY_CLOVER : public REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER
		{
		  public:
			REFIT_VOLUME     *Volume;
			XStringW         VolName;
			apd<EFI_DEVICE_PATH*> DevicePath;
			UINT16            Flags;

			REFIT_MENU_ENTRY_CLOVER() : REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER(), Volume(NULL), VolName(), DevicePath(NULL), Flags(0)  {};
      REFIT_MENU_ENTRY_CLOVER(const REFIT_MENU_ENTRY_CLOVER&) = delete;
      REFIT_MENU_ENTRY_CLOVER& operator=(const REFIT_MENU_ENTRY_CLOVER&) = delete;

			REFIT_MENU_ENTRY_CLOVER* getPartiallyDuplicatedEntry() const;
			virtual REFIT_MENU_ENTRY_CLOVER* getREFIT_MENU_ENTRY_CLOVER() { return this; };
		};


		//---------------------------------------  REFIT_MENU_ITEM_BOOTNUM  ---------------------------------------//

		class REFIT_MENU_ITEM_BOOTNUM : public REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER
		{
		  public:
			REFIT_VOLUME     *Volume;
			UINTN             BootNum;

			REFIT_MENU_ITEM_BOOTNUM() : REFIT_MENU_ITEM_ABSTRACT_ENTRY_LOADER(), Volume(NULL), BootNum(0) {};
      REFIT_MENU_ITEM_BOOTNUM(const REFIT_MENU_ITEM_BOOTNUM&) = delete;
      REFIT_MENU_ITEM_BOOTNUM& operator=(const REFIT_MENU_ITEM_BOOTNUM&) = delete;
			virtual REFIT_MENU_ITEM_BOOTNUM* getREFIT_MENU_ITEM_BOOTNUM() { return this; };
		} ;


			//---------------------------------------  LEGACY_ENTRY  ---------------------------------------//

			class LEGACY_ENTRY : public REFIT_MENU_ITEM_BOOTNUM
			{
			  public:
				LEGACY_ENTRY() : REFIT_MENU_ITEM_BOOTNUM() {};
        
        void StartLegacy();

				virtual LEGACY_ENTRY* getLEGACY_ENTRY() { return this; };
			};


			//---------------------------------------  LOADER_ENTRY  ---------------------------------------//

			class LOADER_ENTRY : public REFIT_MENU_ITEM_BOOTNUM
			{
			  public:
				EFI_GUID     APFSTargetUUID;

				XStringW          DisplayedVolName;
				XStringW          OSName = XStringW();
				apd<EFI_DEVICE_PATH*> DevicePath;
				UINT16            Flags;
				UINT8             LoaderType;
				MacOsVersion      macOSVersion;
				XString8          BuildVersion;
				EFI_GRAPHICS_OUTPUT_BLT_PIXEL BootBgColor;

				UINT8             CustomBoot;
				XImage            CustomLogo;
				KERNEL_AND_KEXT_PATCHES KernelAndKextPatches;
				XStringW          Settings;
        UINT8             *KernelData;
        UINT32            AddrVtable;
        UINT32            SizeVtable;
        UINT32            NamesTable;
        INT32             SegVAddr;
        INT32             shift;
        XBool             PatcherInited;
        XBool             gSNBEAICPUFixRequire; // SandyBridge-E AppleIntelCpuPowerManagement patch require or not
        XBool             gBDWEIOPCIFixRequire; // Broadwell-E IOPCIFamily fix require or not
        XBool             isKernelcache;
        XBool             is64BitKernel;
        UINT32            KernelSlide;
        UINT32            KernelOffset;
        // notes:
        // - 64bit segCmd64->vmaddr is 0xffffff80xxxxxxxx and we are taking
        //   only lower 32bit part into PrelinkTextAddr
        // - PrelinkTextAddr is segCmd64->vmaddr + KernelRelocBase
        UINT32            PrelinkTextLoadCmdAddr;
        UINT32            PrelinkTextAddr;
        UINT32            PrelinkTextSize;
        
        // notes:
        // - 64bit sect->addr is 0xffffff80xxxxxxxx and we are taking
        //   only lower 32bit part into PrelinkInfoAddr ... Why???
        // - PrelinkInfoAddr is sect->addr + KernelRelocBase
        UINT32            PrelinkInfoLoadCmdAddr;
        UINT32            PrelinkInfoAddr;
        UINT32            PrelinkInfoSize;
        EFI_PHYSICAL_ADDRESS    KernelRelocBase;
        BootArgs1         *bootArgs1;
        BootArgs2         *bootArgs2;
        CHAR8             *dtRoot;
        UINT32            *dtLength;
        

				LOADER_ENTRY()
						: REFIT_MENU_ITEM_BOOTNUM(), APFSTargetUUID(), DisplayedVolName(), DevicePath(0), Flags(0), LoaderType(0), macOSVersion(), BuildVersion(),
              BootBgColor({0,0,0,0}),
              CustomBoot(0), CustomLogo(), KernelAndKextPatches(), Settings(), KernelData(0),
              AddrVtable(0), SizeVtable(0), NamesTable(0), SegVAddr(0), shift(0),
              PatcherInited(false), gSNBEAICPUFixRequire(false), gBDWEIOPCIFixRequire(false), isKernelcache(false), is64BitKernel(false),
              KernelSlide(0), KernelOffset(0), PrelinkTextLoadCmdAddr(0), PrelinkTextAddr(0), PrelinkTextSize(0),
              PrelinkInfoLoadCmdAddr(0), PrelinkInfoAddr(0), PrelinkInfoSize(0),
              KernelRelocBase(0), bootArgs1(0), bootArgs2(0), dtRoot(0), dtLength(0)
						{};
        LOADER_ENTRY(const LOADER_ENTRY&) = delete;
        LOADER_ENTRY& operator=(const LOADER_ENTRY&) = delete;
        ~LOADER_ENTRY() {};
        
        void          SetKernelRelocBase();
        void          FindBootArgs();
        EFI_STATUS    getVTable();
        void          Get_PreLink();
        UINT32        Get_Symtab(UINT8*  binary);
        UINT32        GetTextExec();
        UINTN         searchProc(const XString8& procedure);
        UINTN         searchProcInDriver(UINT8 * driver, UINT32 driverLen, const XString8& procedure);
        UINT32        searchSectionByNum(UINT8 * Binary, UINT32 Num);
//        void          KernelAndKextsPatcherStart();
//        void          KernelAndKextPatcherInit();
        XBool         KernelUserPatch();
        XBool         KernelPatchPm();
        XBool         KernelLapicPatch_32();
        XBool         KernelLapicPatch_64();
        XBool         BooterPatch(IN UINT8 *BooterData, IN UINT64 BooterSize);
        void EFIAPI   KernelBooterExtensionsPatch();
        XBool         KernelPanicNoKextDump();
        void          KernelCPUIDPatch();
        XBool         PatchCPUID(const UINT8* Location, INT32 LenLoc,
                                 const UINT8* Search4, const UINT8* Search10, const UINT8* ReplaceModel,
                                 const UINT8* ReplaceExt, INT32 Len);
        void          KernelPatcher_32();
 //       void          KernelPatcher_64();
        void          FilterKernelPatches();
        void          FilterKextPatches();
        void          FilterBootPatches();
        void          applyKernPatch(const UINT8 *find, UINTN size, const UINT8 *repl, const CHAR8 *comment);
        
        EFI_STATUS    SetFSInjection();
//        EFI_STATUS    InjectKexts(IN UINT32 deviceTreeP, IN UINT32 *deviceTreeLength);
//        EFI_STATUS    LoadKexts();
 //       int           is_mkext_v1(UINT8* drvPtr);
 //       void          patch_mkext_v1(UINT8 *drvPtr); //not used
 
//        EFI_STATUS LoadKext(const EFI_FILE *RootDir, const XString8& FileName, IN cpu_type_t archCpuType, IN OUT void *kext);
//        EFI_STATUS AddKext(const EFI_FILE *RootDir, const XString8& FileName, IN cpu_type_t archCpuType);
//        void      LoadPlugInKexts(const EFI_FILE *RootDir, const XString8& DirName, IN cpu_type_t archCpuType, IN XBool Force);
//        void      AddKexts(const XStringW& SrcDir, const XStringW& Path, cpu_type_t archCpuType);
        void      AddKextsFromDirInArray(const XString8& SrcDir, cpu_type_t archCpuType, XObjArray<SIDELOAD_KEXT>* kextArray);
        void      AddKextsInArray(XObjArray<SIDELOAD_KEXT>* kextArray);
        void      AddForceKextsInArray(XObjArray<SIDELOAD_KEXT>* kextArray);
        void      KextPatcherRegisterKexts(void *FSInject, void *ForceLoadKexts);
        void      KextPatcherStart();
        void      PatchPrelinkedKexts();
        void      PatchLoadedKexts();
        void      PatchKext(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize);
        void      AnyKextPatch(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize, size_t N);
        void      ATIConnectorsPatchInit();
        void      ATIConnectorsPatch(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize);
        void      ATIConnectorsPatchRegisterKexts(void *FSInject_v, void *ForceLoadKexts_v);
        void      AppleIntelCPUPMPatch(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize);
        void      AppleRTCPatch(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize);
 //       void      CheckForFakeSMC(CHAR8 *InfoPlist);
        void      DellSMBIOSPatch(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize);
        void      SNBE_AICPUPatch(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize);
        void      BDWE_IOPCIPatch(UINT8 *Driver, UINT32 DriverSize, CHAR8 *InfoPlist, UINT32 InfoPlistSize);
        XBool     SandyBridgeEPM();
        XBool     HaswellEXCPM();
        XBool     HaswellLowEndXCPM();
        XBool     BroadwellEPM();
        XBool     KernelIvyBridgeXCPM();
        XBool     KernelIvyE5XCPM();
        void      EightApplePatch(UINT8 *Driver, UINT32 DriverSize);
        
        void Stall(int Pause) { if ( KernelAndKextPatches.KPDebug ) { gBS->Stall(Pause); } };
        void StartLoader();
        void AddDefaultMenu();
				LOADER_ENTRY* getPartiallyDuplicatedEntry() const;
				virtual LOADER_ENTRY* getLOADER_ENTRY() { return this; };
        LOADER_ENTRY* SubMenuKextInjectMgmt();
        void DelegateKernelPatches();
        
        XBool checkOSBundleRequired(const TagDict* dict);
        XStringW getKextPlist(const EFI_FILE* Root, const XStringW& dirPath, const XStringW& FileName, XBool* NoContents);
        TagDict* getInfoPlist(const EFI_FILE* Root, const XStringW& infoPlistPath);
        XString8 getKextExecPath(const EFI_FILE* Root, const XStringW& dirPath, const XStringW& FileName, TagDict* dict, XBool NoContents);
			} ;


#endif
/*
 
 EOF */
