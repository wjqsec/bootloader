/*
 * cpu.h
 *
 *  Created on: 16 Apr 2020
 *      Author: jief
 */

#ifndef PLATFORM_CPU_H_
#define PLATFORM_CPU_H_

#include "platformdata.h"
#include <IndustryStandard/CpuId.h>
#include <Register/Intel/ArchitecturalMsr.h>
#include <IndustryStandard/ProcessorInfo.h>
#include "../include/OC.h"

#define CPU_MODEL_PENTIUM_M     0x09
#define CPU_MODEL_DOTHAN        0x0D
#define CPU_MODEL_YONAH         0x0E
#define CPU_MODEL_MEROM         0x0F  /* same as CONROE but mobile */
#define CPU_MODEL_CONROE        0x0F  /* Allendale, Conroe, Kentsfield, Woodcrest, Clovertown, Tigerton */
#define CPU_MODEL_CELERON       0x16  /* ever see? */
#define CPU_MODEL_PENRYN        0x17  /* Yorkfield, Harpertown, Penryn M */
#define CPU_MODEL_WOLFDALE      0x17  /* kind of penryn but desktop */
#define CPU_MODEL_NEHALEM       0x1A  /* Bloomfield. Nehalem-EP, Nehalem-WS, Gainestown */
#define CPU_MODEL_ATOM          0x1C  /* Pineview UN */
#define CPU_MODEL_XEON_MP       0x1D  /* MP 7400 UN */
#define CPU_MODEL_FIELDS        0x1E  /* Lynnfield, Clarksfield, Jasper */
#define CPU_MODEL_DALES         0x1F  /* Havendale, Auburndale */
#define CPU_MODEL_CLARKDALE     0x25  /* Clarkdale, Arrandale */
#define CPU_MODEL_ATOM_SAN      0x26  /* Haswell H ? */
#define CPU_MODEL_LINCROFT      0x27  /* UN */
#define CPU_MODEL_SANDY_BRIDGE  0x2A
#define CPU_MODEL_WESTMERE      0x2C  /* Gulftown LGA1366 */
#define CPU_MODEL_JAKETOWN      0x2D  /* Sandy Bridge Xeon LGA2011 */
#define CPU_MODEL_NEHALEM_EX    0x2E
#define CPU_MODEL_WESTMERE_EX   0x2F
#define CPU_MODEL_ATOM_Z8000    0x35
#define CPU_MODEL_ATOM_2000     0x36  /* UN */
#define CPU_MODEL_ATOM_3700     0x37  /* Bay Trail */
#define CPU_MODEL_IVY_BRIDGE    0x3A
#define CPU_MODEL_HASWELL       0x3C  /* Haswell DT */
#define CPU_MODEL_HASWELL_U5    0x3D  /* Haswell U5  5th generation Broadwell*/
#define CPU_MODEL_IVY_BRIDGE_E5 0x3E  /* Ivy Bridge Xeon E5-2650v2 */
#define CPU_MODEL_HASWELL_E     0x3F  /* Haswell Extreme */
//#define CPU_MODEL_HASWELL_H    0x??  // Haswell H
#define CPU_MODEL_HASWELL_ULT   0x45  /* Haswell ULT */
#define CPU_MODEL_CRYSTALWELL   0x46  /* Haswell ULX CPUID_MODEL_CRYSTALWELL */
#define CPU_MODEL_BROADWELL_HQ  0x47  /* E3-1200 v4 5th */
#define CPU_MODEL_MERRIFIELD    0x4A  /* Tangier */
#define CPU_MODEL_AIRMONT       0x4C  /* CherryTrail / Braswell */
#define CPU_MODEL_AVOTON        0x4D  /* Avaton/Rangely */
#define CPU_MODEL_SKYLAKE_U     0x4E  /* Skylake Mobile */
#define CPU_MODEL_BROADWELL_E5  0x4F  /* Xeon E5-2695 5th */
#define CPU_MODEL_SKYLAKE_S     0x55  /* Skylake Server, Cooper Lake, Xeon(R) W-2140B */
#define CPU_MODEL_BROADWELL_DE  0x56  /* Xeon BroadWell 5th */
#define CPU_MODEL_KNIGHT        0x57  /* Knights Landing */
#define CPU_MODEL_MOOREFIELD    0x5A  /* Annidale */
#define CPU_MODEL_GOLDMONT      0x5C  /* Apollo Lake */
#define CPU_MODEL_ATOM_X3       0x5D  /* Silvermont */
#define CPU_MODEL_SKYLAKE_D     0x5E  /* Skylake Desktop */
#define CPU_MODEL_DENVERTON     0x5F  /* Goldmont Microserver */
#define CPU_MODEL_CANNONLAKE    0x66  /* 8h generation Cannon Lake */
#define CPU_MODEL_ICELAKE_A     0x6A  /* Xeon Ice Lake */
#define CPU_MODEL_ICELAKE_C     0x6C  /* Xeon Ice Lake */
#define CPU_MODEL_ATOM_GM       0x7A  /* Goldmont Plus */
#define CPU_MODEL_ICELAKE_D     0x7D  /* 10h Ice Lake */
#define CPU_MODEL_ICELAKE       0x7E  /* 10h Ice Lake */
#define CPU_MODEL_XEON_MILL     0x85  /* Knights Mill */
#define CPU_MODEL_ATOM_TM       0x86  /* Tremont */
#define CPU_MODEL_TIGERLAKE_C   0x8C  /* 11h generation Tiger Lake */
#define CPU_MODEL_TIGERLAKE_D   0x8D  /* 11h generation Tiger Lake */
#define CPU_MODEL_KABYLAKE1     0x8E  /* 7h Kabylake Mobile */
#define CPU_MODEL_ALDERLAKE     0x97  /* 12h generation Alder Lake */
#define CPU_MODEL_ALDERLAKE_ULT 0x9A  /* 12h generation Alder Lake, i5-12500h */
#define CPU_MODEL_KABYLAKE2     0x9E  /* 7h CoffeeLake */
#undef CPU_MODEL_COMETLAKE_S // Jief : mistake in ProcessorInfo.h ?
#define CPU_MODEL_COMETLAKE_S   0x9F  /* desktop Comet Lake */
#define CPU_MODEL_COMETLAKE_Y   0xA5  /* 10h Comet Lake */
#define CPU_MODEL_COMETLAKE_U   0xA6  /* 10h Comet Lake */
#define CPU_MODEL_ROCKETLAKE    0xA7  /* 11h Rocket Lake */
#define CPU_MODEL_METEORLAKE    0xAA  /* 14h Meteor Lake */
#define CPU_MODEL_RAPTORLAKE    0xB7  /* 13h Raptor Lake */
#define CPU_MODEL_RAPTORLAKE_B  0xBF  /* 13h Raptor Lake, i5-13400h */


#define CPU_VENDOR_INTEL        0x756E6547
#define CPU_VENDOR_AMD          0x68747541
/* Unknown CPU */
const char CPU_STRING_UNKNOWN[] = "Unknown CPU Type";


/* CPU defines */

/*
 * The CPUID_FEATURE_XXX values define 64-bit values
 * returned in %ecx:%edx to a CPUID request with %eax of 1:
 */
//#define  CPUID_FEATURE_FPU      _Bit(0)  /* Floating point unit on-chip */
//#define  CPUID_FEATURE_VME      _Bit(1)  /* Virtual Mode Extension */
//#define  CPUID_FEATURE_DE       _Bit(2)  /* Debugging Extension */
//#define  CPUID_FEATURE_PSE      _Bit(3)  /* Page Size Extension */
//#define  CPUID_FEATURE_TSC      _Bit(4)  /* Time Stamp Counter */
//#define  CPUID_FEATURE_MSR      _Bit(5)  /* Model Specific Registers */
//#define CPUID_FEATURE_PAE       _Bit(6)  /* Physical Address Extension */
//#define  CPUID_FEATURE_MCE      _Bit(7)  /* Machine Check Exception */
//#define  CPUID_FEATURE_CX8      _Bit(8)  /* CMPXCHG8B */
//#define  CPUID_FEATURE_APIC     _Bit(9)  /* On-chip APIC */
//#define CPUID_FEATURE_SEP       _Bit(11)  /* Fast System Call */
//#define  CPUID_FEATURE_MTRR     _Bit(12)  /* Memory Type Range Register */
//#define  CPUID_FEATURE_PGE      _Bit(13)  /* Page Global Enable */
//#define  CPUID_FEATURE_MCA      _Bit(14)  /* Machine Check Architecture */
//#define  CPUID_FEATURE_CMOV     _Bit(15)  /* Conditional Move Instruction */
//#define CPUID_FEATURE_PAT       _Bit(16)  /* Page Attribute Table */
//#define CPUID_FEATURE_PSE36     _Bit(17)  /* 36-bit Page Size Extension */
//#define CPUID_FEATURE_PSN       _Bit(18)  /* Processor Serial Number */
//#define CPUID_FEATURE_CLFSH     _Bit(19)  /* CLFLUSH Instruction supported */
//#define CPUID_FEATURE_DS        _Bit(21)  /* Debug Store */
//#define CPUID_FEATURE_ACPI      _Bit(22)  /* Thermal monitor and Clock Ctrl */
//#define CPUID_FEATURE_MMX       _Bit(23)  /* MMX supported */
//#define CPUID_FEATURE_FXSR      _Bit(24)  /* Fast floating pt save/restore */
//#define CPUID_FEATURE_SSE       _Bit(25)  /* Streaming SIMD extensions */
//#define CPUID_FEATURE_SSE2      _Bit(26)  /* Streaming SIMD extensions 2 */
//#define CPUID_FEATURE_SS        _Bit(27)  /* Self-Snoop */
//#define CPUID_FEATURE_HTT       _Bit(28)  /* Hyper-Threading Technology */
//#define CPUID_FEATURE_TM        _Bit(29)  /* Thermal Monitor (TM1) */
//#define CPUID_FEATURE_PBE       _Bit(31)  /* Pend Break Enable */
//
//#define CPUID_FEATURE_SSE3      _HBit(0)  /* Streaming SIMD extensions 3 */
//#define CPUID_FEATURE_PCLMULQDQ _HBit(1) /* PCLMULQDQ Instruction */
//#define CPUID_FEATURE_DTES64    _HBit(2)  /* 64-bit DS layout */
//#define CPUID_FEATURE_MONITOR   _HBit(3)  /* Monitor/mwait */
//#define CPUID_FEATURE_DSCPL     _HBit(4)  /* Debug Store CPL */
//#define CPUID_FEATURE_VMX       _HBit(5)  /* VMX */
//#define CPUID_FEATURE_SMX       _HBit(6)  /* SMX */
//#define CPUID_FEATURE_EST       _HBit(7)  /* Enhanced SpeedsTep (GV3) */
//#define CPUID_FEATURE_TM2       _HBit(8)  /* Thermal Monitor 2 */
//#define CPUID_FEATURE_SSSE3     _HBit(9)  /* Supplemental SSE3 instructions */
//#define CPUID_FEATURE_CID       _HBit(10)  /* L1 Context ID */
//#define CPUID_FEATURE_SEGLIM64  _HBit(11) /* 64-bit segment limit checking */
//#define CPUID_FEATURE_CX16      _HBit(13)  /* CmpXchg16b instruction */
//#define CPUID_FEATURE_xTPR      _HBit(14)  /* Send Task PRiority msgs */
//#define CPUID_FEATURE_PDCM      _HBit(15)  /* Perf/Debug Capability MSR */
//
//#define CPUID_FEATURE_PCID      _HBit(17) /* ASID-PCID support */
//#define CPUID_FEATURE_DCA       _HBit(18)  /* Direct Cache Access */
//#define CPUID_FEATURE_SSE4_1    _HBit(19)  /* Streaming SIMD extensions 4.1 */
//#define CPUID_FEATURE_SSE4_2    _HBit(20)  /* Streaming SIMD extensions 4.2 */
//#define CPUID_FEATURE_xAPIC     _HBit(21)  /* Extended APIC Mode */
//#define CPUID_FEATURE_MOVBE     _HBit(22) /* MOVBE instruction */
//#define CPUID_FEATURE_POPCNT    _HBit(23)  /* POPCNT instruction */
//#define CPUID_FEATURE_TSCTMR    _HBit(24) /* TSC deadline timer */
//#define CPUID_FEATURE_AES       _HBit(25)  /* AES instructions */
//#define CPUID_FEATURE_XSAVE     _HBit(26) /* XSAVE instructions */
//#define CPUID_FEATURE_OSXSAVE   _HBit(27) /* XGETBV/XSETBV instructions */
//#define CPUID_FEATURE_AVX1_0    _HBit(28) /* AVX 1.0 instructions */
//#define CPUID_FEATURE_RDRAND    _HBit(29) /* RDRAND instruction */
//#define CPUID_FEATURE_F16C      _HBit(30) /* Float16 convert instructions */
//#define CPUID_FEATURE_VMM       _HBit(31)  /* VMM (Hypervisor) present */

/*
 * Leaf 7, subleaf 0 additional features.
 * Bits returned in %ebx to a CPUID request with {%eax,%ecx} of (0x7,0x0}:
 */
#define CPUID_LEAF7_FEATURE_RDWRFSGS _Bit(0)  /* FS/GS base read/write */
#define CPUID_LEAF7_FEATURE_SMEP     _Bit(7)  /* Supervisor Mode Execute Protect */
#define CPUID_LEAF7_FEATURE_ENFSTRG  _Bit(9)  /* ENhanced Fast STRinG copy */


/*
 * The CPUID_EXTFEATURE_XXX values define 64-bit values
 * returned in %ecx:%edx to a CPUID request with %eax of 0x80000001:
 */
//#define CPUID_EXTFEATURE_SYSCALL   _Bit(11)  /* SYSCALL/sysret */
//#define CPUID_EXTFEATURE_XD        _Bit(20)  /* eXecute Disable */
//#define CPUID_EXTFEATURE_1GBPAGE   _Bit(26)     /* 1G-Byte Page support */ // ATTENTION : seems wrong. It's BIT21 in Cpuid.h. Not used in Clover.
//#define CPUID_EXTFEATURE_RDTSCP    _Bit(27)  /* RDTSCP */
//#define CPUID_EXTFEATURE_EM64T     _Bit(29)  /* Extended Mem 64 Technology */
//
////#define CPUID_EXTFEATURE_LAHF    _HBit(20)  /* LAFH/SAHF instructions */
//// New definition with Snow kernel
//#define CPUID_EXTFEATURE_LAHF      _HBit(0)  /* LAHF/SAHF instructions */
///*
// * The CPUID_EXTFEATURE_XXX values define 64-bit values
// * returned in %ecx:%edx to a CPUID request with %eax of 0x80000007:
// */
//#define CPUID_EXTFEATURE_TSCI      _Bit(8)  /* TSC Invariant */

#define  CPUID_CACHE_SIZE  16  /* Number of descriptor values */

#define CPUID_MWAIT_EXTENSION  _Bit(0)  /* enumeration of WMAIT extensions */
#define CPUID_MWAIT_BREAK  _Bit(1)  /* interrupts are break events     */

/* Known MSR registers */
//#define MSR_IA32_PLATFORM_ID        0x0017
//#define IA32_APIC_BASE              0x001B  /* used also for AMD */
//#define MSR_CORE_THREAD_COUNT       0x0035   /* limited use - not for Penryn or older  */
//#define IA32_TSC_ADJUST             0x003B
//#define MSR_IA32_BIOS_SIGN_ID       0x008B   /* microcode version */
#define MSR_FSB_FREQ                0x00CD   /* limited use - not for i7            */
/*
•  101B: 100 MHz (FSB 400)
•  001B: 133 MHz (FSB 533)
•  011B: 167 MHz (FSB 667)
•  010B: 200 MHz (FSB 800)
•  000B: 267 MHz (FSB 1067)
•  100B: 333 MHz (FSB 1333)
•  110B: 400 MHz (FSB 1600)
 */
// T8300 -> 0x01A2 => 200MHz
#define  MSR_PLATFORM_INFO          0x00CE   /* limited use - MinRatio for i7 but Max for Yonah  */
                                             /* turbo for penryn */
//haswell
//Low Frequency Mode. LFM is Pn in the P-state table. It can be read at MSR CEh [47:40].
//Minimum Frequency Mode. MFM is the minimum ratio supported by the processor and can be read from MSR CEh [55:48].
#define MSR_PKG_CST_CONFIG_CONTROL  0x00E2   /* sandy and up */
//#define MSR_PMG_IO_CAPTURE_BASE     0x00E4  /* sandy and up */
#define IA32_MPERF                  0x00E7   /* TSC in C0 only */
#define IA32_APERF                  0x00E8   /* actual clocks in C0 */
//#define MSR_IA32_EXT_CONFIG         0x00EE   /* limited use - not for i7            */
//#define MSR_FLEX_RATIO              0x0194   /* limited use - not for Penryn or older      */
                                             //see no value on most CPUs
//#define MSR_IA32_PERF_STATUS        0x0198
//#define MSR_IA32_PERF_CONTROL       0x0199
//#define MSR_IA32_CLOCK_MODULATION   0x019A
#define MSR_THERMAL_STATUS          0x019C
//#define MSR_IA32_MISC_ENABLE        0x01A0
#define MSR_THERMAL_TARGET          0x01A2   /* TjMax limited use - not for Penryn or older      */
#define MSR_TURBO_RATIO_LIMIT       0x01AD   /* limited use - not for Penryn or older      */
//#define MSR_MISC_PWR_MGMT           0x01AA   /* EIST Hardware Coordination Disable (R/W) */
/* defined for Goldmont, Nehalem, Sandy and up
 * bit0=1 == disable
 * bit1=1 == enable MSR 1B0
 */

#define IA32_ENERGY_PERF_BIAS       0x01B0  /* 0=fast 15=low energy If CPUID.6H:ECX[3] = 1 */
//MSR 000001B0                                      0000-0000-0000-0005
#define MSR_PACKAGE_THERM_STATUS    0x01B1
//MSR 000001B1                                      0000-0000-8838-0000
#define IA32_PLATFORM_DCA_CAP       0x01F8
//MSR 000001FC                                      0000-0000-0004-005F


// Sandy Bridge & JakeTown specific 'Running Average Power Limit' MSR's.
#define MSR_RAPL_POWER_UNIT         0x606     /* R/O */
//MSR 00000606                                      0000-0000-000A-1003
#define MSR_PKGC3_IRTL              0x60A    /* RW time limit to go C3 */
          // bit 15 = 1 -- the value valid for C-state PM
#define MSR_PKGC6_IRTL              0x60B    /* RW time limit to go C6 */
//MSR 0000060B                                      0000-0000-0000-8854
  //Valid + 010=1024ns + 0x54=84mks
#define MSR_PKGC7_IRTL              0x60C    /* RW time limit to go C7 */
//MSR 0000060C                                      0000-0000-0000-8854
#define MSR_PKG_C2_RESIDENCY        0x60D   /* same as TSC but in C2 only */

#define MSR_PKG_RAPL_POWER_LIMIT    0x610
//MSR 00000610                                      0000-A580-0000-8960
#define MSR_PKG_ENERGY_STATUS       0x611
//MSR 00000611                                      0000-0000-3212-A857
#define MSR_PKG_POWER_INFO          0x614
//MSR 00000614                                      0000-0000-01E0-02F8
// Sandy Bridge IA (Core) domain MSR's.
#define MSR_PP0_POWER_LIMIT         0x638
#define MSR_PP0_ENERGY_STATUS       0x639
#define MSR_PP0_POLICY              0x63A
#define MSR_PP0_PERF_STATUS         0x63B

// Sandy Bridge Uncore (IGPU) domain MSR's (Not on JakeTown).
#define MSR_PP1_POWER_LIMIT         0x640
#define MSR_PP1_ENERGY_STATUS       0x641
//MSR 00000641                                      0000-0000-0000-0000
#define MSR_PP1_POLICY              0x642

// JakeTown only Memory MSR's.
#define MSR_PKG_PERF_STATUS         0x613
#define MSR_DRAM_POWER_LIMIT        0x618
#define MSR_DRAM_ENERGY_STATUS      0x619
#define MSR_DRAM_PERF_STATUS        0x61B
#define MSR_DRAM_POWER_INFO         0x61C

//IVY_BRIDGE
#define MSR_CONFIG_TDP_NOMINAL      0x648
#define MSR_CONFIG_TDP_LEVEL1       0x649
#define MSR_CONFIG_TDP_LEVEL2       0x64A
#define MSR_CONFIG_TDP_CONTROL      0x64B  /* write once to lock */
#define MSR_TURBO_ACTIVATION_RATIO  0x64C

//Skylake
#define BASE_ART_CLOCK_SOURCE   24000000ULL  /* 24Mhz */
//#define MSR_IA32_PM_ENABLE          0x770
//#define MSR_IA32_HWP_REQUEST        0x774

//AMD
#define K8_FIDVID_STATUS            0xC0010042
#define K10_COFVID_LIMIT            0xC0010061 /* max enabled p-state (msr >> 4) & 7 */
#define K10_COFVID_CONTROL          0xC0010062 /* switch to p-state */
#define K10_PSTATE_STATUS           0xC0010064
#define K10_COFVID_STATUS           0xC0010071 /* current p-state (msr >> 16) & 7 */
/* specific settings
static void SavePState(unsigned int index, unsigned int lowMsr, unsigned int core)
{
  CONST unsigned int msrIndex = 0xC0010064u + index;
  CONST DWORD_PTR affinityMask = (DWORD_PTR)1 << core;

  DWORD lower, higher;
  RdmsrTx(msrIndex, &lower, &higher, affinityMask);

  CONST DWORD lowMsrMask = 0xFE40FFFFu;
  lower = (lower & ~lowMsrMask) | (lowMsr & lowMsrMask);

  WrmsrTx(msrIndex, lower, higher, affinityMask);
}

MSR C0010064  8000-0185-0000-1418 [20.00x] [1.4250 V] [13.30 A] [PState Pb0]
MSR C0010065  8000-0185-0000-1615 [18.50x] [1.4125 V] [13.30 A] [PState Pb1]
MSR C0010066  8000-0173-0000-1A1A [21.00x] [1.3875 V] [11.50 A] [PState P0]
MSR C0010067  0000-0173-0000-1A1A
MSR C0010068  0000-0173-0000-181A
MSR C0010069  0000-0173-0000-1A1A
MSR C001006A  8000-0125-0000-604C [ 7.00x] [0.9500 V] [ 3.70 A] [PState P1]
MSR C001006B  0000-0000-0000-0000
*/


#define DEFAULT_FSB                 100000          /* for now, hardcoding 100MHz for old CPUs */


/* CPUID Index */
#define CPUID_0    0
#define CPUID_1    1
#define CPUID_2    2
#define CPUID_3    3
#define CPUID_4    4
#define CPUID_5    5
#define CPUID_6    6
#define CPUID_7    7
#define CPUID_80   10
#define CPUID_81   11
#define CPUID_87   12
#define CPUID_88   13
#define CPUID_81E  14
#define CPUID_15   15
#define CPUID_MAX  16

/* CPU Cache */
#define MAX_CACHE_COUNT  4
#define CPU_CACHE_LEVEL  3



typedef struct CPU_STRUCTURE {
 //values from CPUID
  UINT32                  CPUID[CPUID_MAX][4] = {{0}};
  UINT32                  Vendor = 0;
  UINT32                  Signature = 0;
  UINT32                  Family = 0;
  UINT32                  Model = 0;
  UINT32                  Stepping = 0;
  UINT32                  Type = 0;
  UINT32                  Extmodel = 0;
  UINT32                  Extfamily = 0;
  UINT64                  Features = 0;
  UINT64                  ExtFeatures = 0;
  UINT32                  CoresPerPackage = 0;
  UINT32                  LogicalPerPackage = 0;
  XString8                BrandString = XString8();

  //values from BIOS
  UINT64                  ExternalClock = 0;
  UINT32                  MaxSpeed = 0;       //MHz
  UINT32                  CurrentSpeed = 0;   //MHz
//  UINT32                  Pad = 0;

  //calculated from MSR
  UINT64                  MicroCode = 0;
  UINT64                  ProcessorFlag = 0;
  UINT32                  MaxRatio = 0;
  UINT32                  SubDivider = 0;
  UINT32                  MinRatio = 0;
  UINT32                  DynFSB = 0;
  UINT64                  ProcessorInterconnectSpeed = 0; //MHz
  UINT64                  FSBFrequency = 0; //Hz
  UINT64                  CPUFrequency = 0;
  UINT64                  TSCFrequency = 0;
  UINT8                   Cores = 0;
  UINT8                   EnabledCores = 0;
  UINT8                   Threads = 0;
  UINT8                   Mobile = 0;  //not for i3-i7
  XBool                   Turbo = false;
  UINT8                   Pad2[3] = {0};

  /* Core i7,5,3 */
  UINT16                  Turbo1 = 0; //1 Core
  UINT16                  Turbo2 = 0; //2 Core
  UINT16                  Turbo3 = 0; //3 Core
  UINT16                  Turbo4 = 0; //4 Core

  UINT64                  TSCCalibr = 0;
  UINT64                  ARTFrequency = 0;

} CPU_STRUCTURE;

#ifndef DONT_DEFINE_GLOBALS
extern UINT64                         TurboMsr;
extern CPU_STRUCTURE                  gCPUStructure;
#endif

void
GetCPUProperties (void);

MacModel
GetDefaultModel ();

UINT16
GetAdvancedCpuType (void);

void
SetCPUProperties (void);

void
FillOCCpuInfo(OC_CPU_INFO* CpuInfo);

#endif /* PLATFORM_CPU_H_ */
