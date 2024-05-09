/*
 * ConfigPlist.h
 *
 *  Created on: Oct 9, 2020
 *      Author: jief
 */

#ifndef _CONFIGPLISTCLASS_CPU_H_
#define _CONFIGPLISTCLASS_CPU_H_


class CPU_Class : public XmlDict
{
  using super = XmlDict;
  
protected:
  class FrequencyMHzClass : public XmlUInt32
  {
      using super = XmlUInt32;
    public:
      virtual XBool validate(XmlLiteParser* xmlLiteParser, const XString8& xmlPath, const XmlParserPosition& keyPos, XBool generateErrors) override {
        RETURN_IF_FALSE( super::validate(xmlLiteParser, xmlPath, keyPos, generateErrors) );
        if ( isDefined()  &&  (value() <= 100 || value() >= 20000) ) {
          return xmlLiteParser->addError(generateErrors, S8Printf("FrequencyMHz must be >200 and <20000 tag '%s:%d'.", xmlPath.c_str(), keyPos.getLine()));
        }
        return true;
      }
  };
  class LatencyClass : public XmlUInt16
  {
      using super = XmlUInt16;
    public:
      virtual XBool validate(XmlLiteParser* xmlLiteParser, const XString8& xmlPath, const XmlParserPosition& keyPos, XBool generateErrors) override {
        RETURN_IF_FALSE( super::validate(xmlLiteParser, xmlPath, keyPos, generateErrors) );
        return xmlLiteParser->addWarning(generateErrors, S8Printf("Latency is replaced by ACPI/SSDT/C3Latency, value ignored. tag '%s:%d'.", xmlPath.c_str(), keyPos.getLine()));
      }
  };

  XmlUInt16 QPI = XmlUInt16();
  FrequencyMHzClass FrequencyMHz = FrequencyMHzClass();
  XmlUInt16 Type = XmlUInt16();
  XmlBool QEMU = XmlBool();
  XmlBool UseARTFrequency = XmlBool();
  XmlUInt32 BusSpeedkHz = XmlUInt32();
  XmlBool C6 = XmlBool();
  XmlBool C4 = XmlBool();
  XmlBool C2 = XmlBool();
  LatencyClass Latency = LatencyClass();
  XmlUInt8 SavingMode = XmlUInt8();
  XmlBool HWPEnable = XmlBool();
  XmlUInt32 HWPValue = XmlUInt32();
  XmlUInt8 TDP = XmlUInt8();
  XmlBool TurboDisable = XmlBool();
  
  XmlDictField m_fields[15] = {
    {"QPI", QPI},
    {"FrequencyMHz", FrequencyMHz},
    {"Type", Type},
    {"QEMU", QEMU},
    {"UseARTFrequency", UseARTFrequency},
    {"BusSpeedkHz", BusSpeedkHz},
    {"C6", C6},
    {"C4", C4},
    {"C2", C2},
    {"Latency", Latency},
    {"SavingMode", SavingMode},
    {"HWPEnable", HWPEnable},
    {"HWPValue", HWPValue},
    {"TDP", TDP},
    {"TurboDisable", TurboDisable},
  };

public:
  virtual void getFields(XmlDictField** fields, size_t* nb) override { *fields = m_fields; *nb = sizeof(m_fields)/sizeof(m_fields[0]); };
  
  CPU_Class() {}
  
  uint16_t dgetQPI() const { return QPI.isDefined() ? QPI.value() : 0; };
  uint32_t dgetCpuFreqMHz() const { return FrequencyMHz.isDefined() ? FrequencyMHz.value() : 0; };
  uint16_t dgetCpuType() const { return Type.isDefined() ? Type.value() : GetAdvancedCpuType(); };
  XBool dgetQEMU() const { return QEMU.isDefined() ? QEMU.value() : XBool(false); };
  XBool dgetUseARTFreq() const { return UseARTFrequency.isDefined() ? UseARTFrequency.value() : XBool(false); };
  uint32_t dgetBusSpeed() const { return BusSpeedkHz.isDefined() ? BusSpeedkHz.value() : 0; };
  undefinable_bool dget_EnableC6() const { return C6.isDefined() ? undefinable_bool(C6.value()) : undefinable_bool(); };
  undefinable_bool dget_EnableC4() const { return C4.isDefined() ? undefinable_bool(C4.value()) : undefinable_bool(); };
  undefinable_bool dget_EnableC2() const { return C2.isDefined() ? undefinable_bool(C2.value()) : undefinable_bool(); };
  undefinable_uint16 dget_C3Latency() const { return Latency.isDefined() ? undefinable_uint16(Latency.value()) : undefinable_uint16(); };
  uint8_t dgetSavingMode() const { return SavingMode.isDefined() ? SavingMode.value() : 0xFF; };
  XBool dgetHWPEnable() const { return HWPEnable.isDefined() ? HWPEnable.value() : XBool(false); };
  undefinable_uint32 dgetHWPValue() const { return HWPValue.isDefined() ? undefinable_uint32(HWPValue.value()) : undefinable_uint32(); };
  uint8_t dgetTDP() const { return TDP.isDefined() ? TDP.value() : false; };
  XBool dgetTurboDisabled() const { return TurboDisable.isDefined() ? TurboDisable.value() : XBool(false); };

  XBool dgetUserChange() const { return BusSpeedkHz.isDefined(); };

  undefinable_bool getUseARTFreq() const { return UseARTFrequency.isDefined() ? undefinable_bool(UseARTFrequency.value()) : undefinable_bool(); };

};


#endif /* _CONFIGPLISTCLASS_CPU_H_ */
