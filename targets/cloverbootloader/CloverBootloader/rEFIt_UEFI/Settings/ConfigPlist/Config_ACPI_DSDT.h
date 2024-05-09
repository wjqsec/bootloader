/*
 * ConfigPlist.h
 *
 *  Created on: Oct 9, 2020
 *      Author: jief
 */

#ifndef _CONFIGPLISTCLASS_ACPI_DSDT_H_
#define _CONFIGPLISTCLASS_ACPI_DSDT_H_


#include "../../include/DsdtFixList.h"


class DSDT_Class : public XmlDict
{
  using super = XmlDict;
public:
    class ACPI_DSDT_Fixes_Class;

    class ACPI_DSDT_Fix : public XmlAbstractType
    {
      using super = XmlAbstractType;
      friend class ACPI_DSDT_Fixes_Class;
    protected:
      const CHAR8* m_newName = NULL;
      const CHAR8* m_oldName = NULL;
      UINT32 m_bitData = 0;
      XmlBool oldEnabled {};
      XmlBool newEnabled {};

    public:
      constexpr ACPI_DSDT_Fix(const CHAR8* newName, const CHAR8* oldName, UINT32 bitData ) : m_newName(newName), m_oldName(oldName), m_bitData(bitData), oldEnabled(), newEnabled() {};
      ACPI_DSDT_Fix(const ACPI_DSDT_Fix&) { panic("copy ctor"); }; // can't do "=delete" because clang thinks it will be called at initialization "ACPI_DSDT_Fixes_Class Fixes = ACPI_DSDT_Fixes_Class();"
      ACPI_DSDT_Fix& operator = (const ACPI_DSDT_Fix&) = delete; // { panic("copy ctor"); }; // = delete;

      virtual const char* getDescription() override { panic("not defined"); };
      virtual XBool isTheNextTag(XmlLiteParser* xmlLiteParser) override { panic("not defined"); };
      virtual XBool parseFromXmlLite(XmlLiteParser* xmlLiteParser, const XString8& xmlPath, XBool generateErrors) override { panic("not defined"); };

      virtual void reset() override {
        super::reset();
        oldEnabled.reset();
        newEnabled.reset();
      };

      constexpr const CHAR8* getNewName() const { return m_newName; }
      const CHAR8* getOldName() const { return m_oldName; }
      uint32_t getBitData() const { return m_bitData; }

      XBool dgetOldEnabled() const { return oldEnabled.isDefined() ? oldEnabled.value() : XBool(false); };
      XBool dgetNewEnabled() const { return newEnabled.isDefined() ? newEnabled.value() : XBool(false); };

      XBool dgetEnabled() const {
        if ( oldEnabled.isDefined() && oldEnabled.value() ) return true;
        if ( newEnabled.isDefined() && newEnabled.value() ) return true;
        return false;
      }
    };

    class ACPI_DSDT_Fixes_Class : public XmlDict
    {
      using super = XmlDict;
    public:
      ACPI_DSDT_Fix ACPI_DSDT_Fixe_Array[31] = { // CAREFUL not to declare too much
        { "AddDTGP_0001", "AddDTGP", FIX_DTGP },
        { "FixDarwin_0002", "FixDarwin", FIX_WARNING },
        { "FixShutdown_0004", "FixShutdown", FIX_SHUTDOWN },
        { "AddMCHC_0008", "AddMCHC", FIX_MCHC },
        { "FixHPET_0010", "FixHPET", FIX_HPET },
        { "FakeLPC_0020", "FakeLPC", FIX_LPC },
        { "FixIPIC_0040", "FixIPIC", FIX_IPIC },
        { "FixSBUS_0080", "FixSBUS", FIX_SBUS },
        { "FixDisplay_0100", "FixDisplay", FIX_DISPLAY },
        { "FixIDE_0200", "FixIDE", FIX_IDE },
        { "FixSATA_0400", "FixSATA", FIX_SATA },
        { "FixFirewire_0800", "FixFirewire", FIX_FIREWIRE },
        { "FixUSB_1000", "FixUSB", FIX_USB },
        { "FixLAN_2000", "FixLAN", FIX_LAN },
        { "FixAirport_4000", "FixAirport", FIX_WIFI },
        { "FixHDA_8000", "FixHDA", FIX_HDA },
        { "FixDarwin7_10000", "FixDarwin7", FIX_DARWIN },
        { "FIX_RTC_20000", "FixRTC", FIX_RTC },
        { "FIX_TMR_40000", "FixTMR", FIX_TMR },
        { "AddIMEI_80000", "AddIMEI", FIX_IMEI },
        { "FIX_INTELGFX_100000", "FixIntelGfx", FIX_INTELGFX },
        { "FIX_WAK_200000", "FixWAK", FIX_WAK },
        { "DeleteUnused_400000", "DeleteUnused", FIX_UNUSED },
        { "FIX_ADP1_800000", "FixADP1", FIX_ADP1 },
        { "AddPNLF_1000000", "AddPNLF", FIX_PNLF },
        { "FIX_S3D_2000000", "FixS3D", FIX_S3D },
        { "FIX_ACST_4000000", "FixACST", FIX_ACST },
        { "AddHDMI_8000000", "AddHDMI", FIX_HDMI },
        { "FixRegions_10000000", "FixRegions", FIX_REGIONS },
        { "FixHeaders_20000000", "FixHeaders", FIX_HEADERS_DEPRECATED },
        { NULL, "FixMutex", FIX_MUTEX }
      };

    public:
      virtual void reset() override {
        super::reset();
        for ( size_t idx = 0 ; idx < sizeof(ACPI_DSDT_Fixe_Array)/sizeof(ACPI_DSDT_Fixe_Array[0]) ; idx++ ) {
          ACPI_DSDT_Fixe_Array[idx].reset();
        }
      };

      virtual XmlAbstractType& parseValueFromXmlLite(XmlLiteParser* xmlLiteParser, const XString8& xmlPath, XBool generateErrors, const XmlParserPosition &keyPos, const char *keyValue, size_t keyValueLength, XBool* keyFound) override;
      
      virtual XBool validate(XmlLiteParser* xmlLiteParser, const XString8& xmlPath, const XmlParserPosition& keyPos, XBool generateErrors) override {
        bool b = super::validate(xmlLiteParser, xmlPath, keyPos, generateErrors);
//        if ( LString8(ACPI_DSDT_Fixe_Array[29].getNewName()) != "FixHeaders_20000000"_XS8 ) {
//          log_technical_bug("ACPI_DSDT_Fixe_Array[29].getNewName() != \"FixHeaders_20000000\"");
//          return true; // Bug in ACPI_DSDT_Fixe_Array. We don't want to reset all the values, so return true.
//        }
//        if ( ACPI_DSDT_Fixe_Array[29].isDefined() ) {
//          xmlLiteParser->addWarning(generateErrors, S8Printf("FixHeaders is ACPI/DSDT in deprecated. Move it to ACPI."));
//          return true; // return true because we handle this value anyway.
//        }
        return b;
      }

      const ACPI_DSDT_Fix& getFixHeaders() const {
        // FixHeaders is bit 29, but that's a coincidence with the index of the array. ACPI_DSDT_Fixe_Array[FIX_HEADERS_DEPRECATED] would be wrong.
 //       if ( LString8(ACPI_DSDT_Fixe_Array[29].getNewName()) != "FixHeaders_20000000"_XS8 ) {
 //         log_technical_bug("ACPI_DSDT_Fixe_Array[29].getNewName() != \"FixHeaders_20000000\"");
 //       }
        return ACPI_DSDT_Fixe_Array[29];
      }

      uint32_t dgetFixBiosDsdt() const {
        uint32_t FixDsdt = 0;
        for (size_t Index = 0; Index < sizeof(ACPI_DSDT_Fixe_Array)/sizeof(ACPI_DSDT_Fixe_Array[0]); Index++) {
          if ( ACPI_DSDT_Fixe_Array[Index].dgetEnabled() ) FixDsdt |= ACPI_DSDT_Fixe_Array[Index].getBitData();
        }
        return FixDsdt;
      }
      XBool dgetFixHeaders() const {
        return getFixHeaders().dgetEnabled();
      }
    };

    class ACPI_DSDT_Patch_Class : public XmlDict
    {
        using super = XmlDict;
      protected:
        class TgtBridgeClass : public XmlData
        {
          using super = XmlData;
          virtual XBool validate(XmlLiteParser* xmlLiteParser, const XString8& xmlPath, const XmlParserPosition& keyPos, XBool generateErrors) override {
#ifdef JIEF_DEBUG
if ( xmlPath.contains("ACPI/DSDT/Patches[15]"_XS8) ) {
  NOP;
}
#endif
            RETURN_IF_FALSE( super::validate(xmlLiteParser, xmlPath, keyPos, generateErrors) );
            if ( isDefined() && value().size() > 0 && value().size() != 4 ) return xmlLiteParser->addWarning(generateErrors, S8Printf("TgtBridge must 4 bytes. It's %zu byte(s). ignored tag '%s:%d'.", value().size(), xmlPath.c_str(), keyPos.getLine()));
            return true;
          }
        };
        
        XmlBool    Disabled = XmlBool();
        XmlString8AllowEmpty Comment = XmlString8AllowEmpty();
        XmlData Find = XmlData();
        XmlData Replace = XmlData();
        TgtBridgeClass TgtBridge = TgtBridgeClass();
        XmlUInt64 Skip = XmlUInt64();
        XmlInt32 Count = XmlInt32();

        XmlDictField m_fields[7] = {
          {"Comment", Comment},
          {"Disabled", Disabled},
          {"Find", Find},
          {"Replace", Replace},
          {"TgtBridge", TgtBridge},
          {"Skip", Skip},
          {"Count", Count},
        };

        virtual void getFields(XmlDictField** fields, size_t* nb) override { *fields = m_fields; *nb = sizeof(m_fields)/sizeof(m_fields[0]); };
      public:
        XBool dgetDisabled() const { return Disabled.isDefined() ? Disabled.value() : XBool(false); };
        uint8_t dgetBValue() const { return Disabled.isDefined() ? Disabled.value() : XBool(false); };
        XString8 dgetPatchDsdtLabel() const { return Comment.isDefined() ? Comment.value() : "(NoLabel)"_XS8; };
        const XBuffer<UINT8>& dgetPatchDsdtFind() const { return Find.isDefined() ? Find.value() : XBuffer<UINT8>::NullXBuffer; };
        const XBuffer<UINT8>& dgetPatchDsdtReplace() const { return Replace.isDefined() ? Replace.value() : XBuffer<UINT8>::NullXBuffer; };
        const XBuffer<UINT8>& dgetPatchDsdtTgt() const { return TgtBridge.isDefined() ? TgtBridge.value() : XBuffer<UINT8>::NullXBuffer; };
        const decltype(Skip)::ValueType& dgetSkip() const { return Skip.isDefined() ? Skip.value() : Skip.nullValue; };
        const decltype(Count)::ValueType& dgetCount() const { return Count.isDefined() ? Count.value() : Count.nullValue; };
    };

protected:
  XmlString8AllowEmpty Name = XmlString8AllowEmpty();
  XmlBool Debug = XmlBool();
  XmlBool Rtc8Allowed = XmlBool();
  XmlUInt8 PNLF_UID = XmlUInt8();
  XmlUInt32 FixMask = XmlUInt32();
public:
  ACPI_DSDT_Fixes_Class Fixes = ACPI_DSDT_Fixes_Class();
  XmlArray<ACPI_DSDT_Patch_Class> Patches = XmlArray<ACPI_DSDT_Patch_Class>();
protected:
  XmlBool ReuseFFFF = XmlBool();
  XmlBool SuspendOverride = XmlBool();
  
  XmlDictField m_fields[9] = {
    {"Name", Name},
    {"Debug", Debug},
    {"Rtc8Allowed", Rtc8Allowed},
    {"PNLF_UID", PNLF_UID},
    {"FixMask", FixMask},
    {"Fixes", Fixes},
    {"Patches", Patches},
    {"ReuseFFFF", ReuseFFFF},
    {"SuspendOverride", SuspendOverride},
  };
public:
  virtual void getFields(XmlDictField** fields, size_t* nb) override { *fields = m_fields; *nb = sizeof(m_fields)/sizeof(m_fields[0]); };
  
  XString8 dgetDsdtName() const { return Name.isDefined() && Name.value().notEmpty() ? Name.value() : "DSDT.aml"_XS8; };
  XBool dgetDebugDSDT() const { return Debug.isDefined() ? Debug.value() : XBool(false); };
  XBool dgetRtc8Allowed() const { return Rtc8Allowed.isDefined() ? Rtc8Allowed.value() : XBool(false); };
  uint8_t dgetPNLF_UID() const { return isDefined() ? PNLF_UID.isDefined() ? PNLF_UID.value() : 0x0a : 0; }; // TODO: different default value if section is not defined
  uint32_t dgetFixDsdt() const {
    // priority is given to Fixes
    if ( Fixes.isDefined() ) return Fixes.dgetFixBiosDsdt();
    return FixMask.isDefined() ? FixMask.value() : 0;
  };
//  const ACPI_DSDT_Fixes_Class& getFixes() const { return Fixes; };
//  const XmlArray<ACPI_DSDT_Patch_Class>& getPatches() const { return Patches; };
  XBool dgetReuseFFFF() const { return ReuseFFFF.isDefined() ? ReuseFFFF.value() : XBool(false); };
  XBool dgetSuspendOverride() const { return SuspendOverride.isDefined() ? SuspendOverride.value() : XBool(false); };

};

#endif /* _CONFIGPLISTCLASS_ACPI_DSDT_H_ */
