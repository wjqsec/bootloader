/*
 * smbios.h
 *
 *  Created on: 16 Apr 2020
 *      Author: jief
 */

#ifndef PLATFORM_SMBIOS_H_
#define PLATFORM_SMBIOS_H_

extern "C" {
#include <IndustryStandard/AppleSmBios.h>
}
//#include "../Settings/ConfigPlist/ConfigPlistClass.h"
#include "../Platform/cpu.h"
//#include "../Platform/Settings.h"

// The maximum number of RAM slots to detect
// even for 3-channels chipset X58 there are no more then 8 slots
const uint8_t MAX_RAM_SLOTS = 24;

// The maximum sane frequency for a RAM module
#define MAX_RAM_FREQUENCY 5000

class RAM_SLOT_INFO {
public:
  UINT32   ModuleSize     = 0;
  UINT32   Frequency      = 0;
  XString8 Vendor         = XString8();
  XString8 PartNo         = XString8();
  XString8 SerialNo       = XString8();
  UINT8    Type           = 0;
  UINT8    SlotIndex      = 0;
//  XBool    InUse = false;

  RAM_SLOT_INFO() {}
};
extern const RAM_SLOT_INFO nullRAM_SLOT_INFO;

class SmbiosMemoryConfigurationClass {
  public:
    UINT8         SlotCount = UINT8();
    UINT8         UserChannels = UINT8();
    XObjArray<RAM_SLOT_INFO> _User = XObjArray<RAM_SLOT_INFO>();

    SmbiosMemoryConfigurationClass() {}

    void setEmpty() {
      SlotCount = 0;
      UserChannels = 0;
      _User.setEmpty();
    }

    XBool doesSlotForIndexExist(uint8_t idx2Look4) const {
      for ( size_t idx = 0 ; idx < _User.size() ; ++idx ) {
        if ( _User[idx].SlotIndex == idx2Look4 ) return true;
      }
      return false;
    }

    const RAM_SLOT_INFO& getSlotInfoForSlotID(UINT64 Slot) const {
      for ( size_t idx = 0 ; idx < _User.size() ; ++idx ) {
        if ( _User[idx].SlotIndex == Slot ) return _User[idx];
      }
      return nullRAM_SLOT_INFO;
    }
};

class SLOT_DEVICE
{
public:
  uint8_t           Index = 0xFF;
  UINT16            SegmentGroupNum = UINT16();
  UINT8             BusNum = UINT8();
  UINT8             DevFuncNum = UINT8();
  UINT8             SlotID = UINT8();
  MISC_SLOT_TYPE    SlotType = MISC_SLOT_TYPE();
  XString8          SlotName = XString8();

  SLOT_DEVICE() {}
};
extern const SLOT_DEVICE nullSLOT_DEVICE;

/*
 * All settings from Smbios goes into this struct.
 * Goal : No globals set by getTablexxx functions
 */
class SmbiosDiscoveredSettings
{
  public:
    uint16_t SmbiosVersion            = 0;
    XString8 OEMBoardFromSmbios       = XString8();
    XString8 OEMProductFromSmbios     = XString8();
    XString8 OEMVendorFromSmbios      = XString8();
    uint8_t  EnabledCores             = 0;
    
    remove_const(decltype(MAX_RAM_SLOTS)) RamSlotCount = 0; // this is discovered in GetTableType16(). Can be max out to MAX_RAM_SLOTS
    static_assert(numeric_limits<decltype(MAX_RAM_SLOTS)>::max() <= numeric_limits<decltype(RamSlotCount)>::max(), "RamSlotCount wrong type");

    // gCPUStructure
    UINT64                ExternalClock = 0;
    UINT32                CurrentSpeed  = 0;
    UINT32                MaxSpeed      = 0;

    SmbiosDiscoveredSettings() {}
};

/*
 * All settings that'll be injected goes into this struct.
 * Goal : No globals used by patchTablexxx functions
 * The method that initialises this is SmbiosFillPatchingValues()
 * Q: Why is this interesting ? Isn't it just copy and we should let smbios.cpp access globals, like gCPUStructure ?
 * A: Problems with globals, is that you don't know  where they are accessed from.
 *    Imagine you have a wrong information sent to Smbios.
 *    Putting a breakpoint or a log in SmbiosInjectedSettings::takeValueFrom, you immediately know if the problem is
 *    on the Clover side (wrong info given by Clover) or on the Smbios patch side (Right info but wrong way of patching smbios table).
 *    This way, Smbios is a layer (or toolbox) independent of Clover.
 *    SmbiosInjectedSettings is a "touch point" (some say "bridge") between layer. Of course there should only be 1 (or 2), easily identifiable "touch point".
 *    SmbiosFillPatchingValues() is THE place to make some checks, gather values and be sure of what to send to the patching functions.
 *
 *    NOTE : I know it's tempting not to do it because it's a lot of copy/paste. But it's so much a time saver later to have better/simpler design...
 */
class SmbiosInjectedSettings
{
  public:
    // gCPUStructure
    UINT8                   Cores = 0;
    UINT32                  MaxSpeed = 0;
    UINT8                   Threads = 0;
    UINT64                  Features = 0;
    UINT64                  ExternalClock = 0;
    UINT32                  Model = 0;
    UINT8                   Mobile = 0;  //not for i3-i7
    UINT32                  Family = 0;
    UINT32                  Type = 0;
    XString8                BrandString = XString8();
    UINT32                  Extmodel = 0;
    UINT64                  ExtFeatures = 0;
    UINT64                  MicroCode = 0;
    UINT32                  Extfamily = 0;
    UINT32                  Stepping = 0;

    // gSettings
    XString8 BiosVendor              = XString8();
    XString8 BiosVersionUsed         = XString8();
    XString8 EfiVersionUsed          = XString8();
    XString8 ReleaseDateUsed         = XString8();
    XString8 ManufactureName         = XString8();
    XString8 ProductName             = XString8();
    XString8 SystemVersion           = XString8();
    XString8 SerialNr                = XString8();
    XString8 BoardNumber             = XString8();
    XString8 BoardManufactureName    = XString8();
    XString8 BoardVersion            = XString8();
    XString8 BoardSerialNumber       = XString8();
    XString8 LocationInChassis       = XString8();
    XString8 ChassisManufacturer     = XString8();
    XString8 ChassisAssetTag         = XString8();
    XString8 FamilyName              = XString8();
    EFI_GUID SmUUID             = EFI_GUID();
    XBool    NoRomInfo = false;
    uint8_t  EnabledCores = 0;
    XBool    TrustSMBIOS = false;
    XBool    InjectMemoryTables = false;
    uint8_t  BoardType = 0;
    uint8_t  ChassisType = 0;

    class SlotDevicesArrayClass : protected XObjArray<SLOT_DEVICE>
    {
        using super = XObjArray<SLOT_DEVICE>;
      public:
        void setEmpty() { super::setEmpty(); }
        void AddReference(SLOT_DEVICE* newElement, XBool FreeIt) { super::AddReference(newElement, FreeIt); }

        const SLOT_DEVICE& getSlotForIndex(size_t Index) const {
          if ( Index >= MAX_RAM_SLOTS) {
            log_technical_bug("%s : Index >= MAX_RAM_SLOTS", __PRETTY_FUNCTION__);
          }
          for ( size_t idx = 0 ; idx < size() ; ++idx ) {
            if ( ElementAt(idx).Index == Index ) return ElementAt(idx);
          }
          return nullSLOT_DEVICE;
        }
        SLOT_DEVICE& getOrCreateSlotForIndex(size_t Index) {
          if ( Index >= MAX_RAM_SLOTS) {
            log_technical_bug("%s : Index >= MAX_RAM_SLOTS", __PRETTY_FUNCTION__);
          }
          for ( size_t idx = 0 ; idx < size() ; ++idx ) {
            if ( ElementAt(idx).Index == Index ) return ElementAt(idx);
          }
          SLOT_DEVICE* slotDevice = new SLOT_DEVICE;
          AddReference(slotDevice, true);
          return *slotDevice;
        }
        XBool isSlotForIndexValid(uint8_t Index) const {
          if ( Index >= MAX_RAM_SLOTS) {
            log_technical_bug("%s : Index >= MAX_RAM_SLOTS", __PRETTY_FUNCTION__);
          }
          for ( size_t idx = 0 ; idx < size() ; ++idx ) {
            if ( ElementAt(idx).Index == Index ) return true;
          }
          return false;
        }
    } SlotDevices         = SlotDevicesArrayClass();
    
    SmbiosMemoryConfigurationClass Memory    = SmbiosMemoryConfigurationClass();

    uint64_t gPlatformFeature = 0;
    uint32_t FirmwareFeatures = 0;
    uint32_t FirmwareFeaturesMask = 0;
    uint64_t ExtendedFirmwareFeatures = 0;
    uint64_t ExtendedFirmwareFeaturesMask = 0;
    int8_t Attribute = 0;

    XBool KPDELLSMBIOS = false;

    // CPU
    uint16_t CpuType = 0;
    XBool SetTable132 = false;
    uint16_t QPI = 0;

    // from SmBios
    uint16_t RamSlotCount = 0;
    
    SmbiosInjectedSettings() {}
};

class RAM_SLOT_INFO_Array
{
  XObjArray<RAM_SLOT_INFO> m_RAM_SLOT_INFO_Array = XObjArray<RAM_SLOT_INFO>();
public:

  const XBool doesSlotForIndexExist(uint8_t idx2Look4) const {
    for ( size_t idx = 0 ; idx < m_RAM_SLOT_INFO_Array.size() ; ++idx ) {
      if ( m_RAM_SLOT_INFO_Array[idx].SlotIndex == idx2Look4 ) return true;
    }
    return false;
  }

  template<typename IntegralType, enable_if(is_integral(IntegralType))>
  const RAM_SLOT_INFO& getSlotInfoForSlotIndex(IntegralType nIndex) {
//DebugLog(1, "SPDArray:[] : index = %lld, size=%zd\n", (uint64_t)nIndex, m_RAM_SLOT_INFO_Array.size());
    if ( nIndex < 0 ) {
      log_technical_bug("%s : nIndex < 0", __PRETTY_FUNCTION__);
      return nullRAM_SLOT_INFO;
    }
    for ( size_t idx = 0 ; idx < m_RAM_SLOT_INFO_Array.size() ; ++idx ) {
      if ( m_RAM_SLOT_INFO_Array[idx].SlotIndex == (unsigned_type(IntegralType))nIndex ) return m_RAM_SLOT_INFO_Array[idx];
    }
    return nullRAM_SLOT_INFO;
  }
  
  size_t getSlotCount() const
  {
    size_t max = 0;
    for ( size_t idx = 0 ; idx < m_RAM_SLOT_INFO_Array.size() ; ++idx ) {
      if ( m_RAM_SLOT_INFO_Array[idx].ModuleSize > 0 ) {
        if ( m_RAM_SLOT_INFO_Array[idx].SlotIndex > max ) max = m_RAM_SLOT_INFO_Array[idx].SlotIndex;
      }
    }
    return max+1;
  };

  size_t size() const { return m_RAM_SLOT_INFO_Array.size(); }

  void AddReference(RAM_SLOT_INFO* rsiPtr, XBool freeIt) { m_RAM_SLOT_INFO_Array.AddReference(rsiPtr, freeIt); }
};

class MEM_STRUCTURE
{
public:
  UINT32        Frequency   = 0;
  UINT32        Divider     = 0;
  UINT8         TRC         = 0;
  UINT8         TRP         = 0;
  UINT8         RAS         = 0;
  UINT8         Channels    = 0;
  UINT8         Slots       = 0;
  UINT8         Type        = 0;

  RAM_SLOT_INFO_Array SPD    = RAM_SLOT_INFO_Array();
  RAM_SLOT_INFO_Array SMBIOS = RAM_SLOT_INFO_Array();

};


extern APPLE_SMBIOS_STRUCTURE_POINTER SmbiosTable;

// TODO stop using globals.
extern MEM_STRUCTURE            gRAM;
extern XBool                    gMobile;



UINTN
iStrLen(
  CONST CHAR8* String,
  UINTN  MaxLen
  );

EFI_STATUS PrepatchSmbios(SmbiosDiscoveredSettings* smbiosSettings);
void PatchSmbios(const SmbiosInjectedSettings& smbiosSettings);
void FinalizeSmbios(const SmbiosInjectedSettings& smbiosSettings);

XBool getMobileFromSmbios();
EFI_GUID getSmUUIDFromSmbios();


extern SmbiosDiscoveredSettings g_SmbiosDiscoveredSettings;
extern SmbiosInjectedSettings g_SmbiosInjectedSettings;

#endif /* PLATFORM_SMBIOS_H_ */
