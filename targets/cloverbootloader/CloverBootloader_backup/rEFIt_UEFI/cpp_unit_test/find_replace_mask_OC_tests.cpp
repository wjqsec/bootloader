#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile

//#include "../../OpenCorePkg/Include/Acidanthera/Library/OcMiscLib.h"

extern "C" {

BOOLEAN
FindPattern (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN UINT32        *DataOff
  );

UINT32
ApplyPatch (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Replace,
  IN CONST UINT8   *ReplaceMask OPTIONAL,
  IN UINT8         *Data,
  IN UINT32        DataSize,
  IN UINT32        Count,
  IN UINT32        Skip
  );

}

static int breakpoint(int i)
{
  return i;
}

int find_replace_mask_OC_tests()
{
//  int ret;
  BOOLEAN b;
  INT32 int32;
  UINT32 uint32;
//  UINT64 uint64;

  uint32 = 0;
  b = FindPattern((UINT8*)"\x11", NULL, 1, (UINT8*)"\x01\x11\x02", 3, &uint32);
  if ( !b ) breakpoint(1);
  if ( uint32 != 1 ) breakpoint(1);

  uint32 = 0;
  int32 = FindPattern((UINT8*)"\x13\x14", NULL, 2, (UINT8*)"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a", 10, &uint32);
  if ( !b ) breakpoint(1);
  if ( int32 != 2 ) breakpoint(1);

  uint32 = 0;
  int32 = FindPattern((UINT8*)"\x14\x00\x16", (UINT8*)"\xFF\x00\xFF", 3, (UINT8*)"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a", 10, &uint32);
  if ( !b ) breakpoint(1);
  if ( int32 != 3 ) breakpoint(1);

  uint32 = 0;
  int32 = FindPattern((UINT8*)"\xC0", (UINT8*)"\xF0", 1, (UINT8*)"\x00\xCC\x00", 3, &uint32);
  if ( !b ) breakpoint(1);
  if ( int32 != 1 ) breakpoint(1);

  /*
   * For OC, the mask pattern must applied to the source, first.
   * 0xCF & 0xF0 = 0xC0
   */
  uint32 = 0;
  int32 = FindPattern((UINT8*)"\xCF", (UINT8*)"\xF0", 1, (UINT8*)"\x00\xCC\x00", 3, &uint32);
  if ( !b ) breakpoint(1);
  if ( int32 != -1 ) breakpoint(1);

  // Search in clever
  uint32 = 0;
  int32 = FindPattern((UINT8*)"\x43\x6c\x65\x76\x65\x72", (UINT8*)"\xDF\xFF\xFF\xFF\xFF\xFF", 6, (UINT8*)"\x01\x63\x6c\x65\x76\x65\x72\x02", 8, &uint32);
  if ( !b ) breakpoint(1);
  if ( int32 != 1 ) breakpoint(1);
  // Search in Clever
  uint32 = 0;
  int32 = FindPattern((UINT8*)"\x43\x6c\x65\x76\x65\x72", (UINT8*)"\xDF\xFF\xFF\xFF\xFF\xFF", 6, (UINT8*)"\x01\x43\x6c\x65\x76\x65\x72\x02", 8, &uint32);
  if ( !b ) breakpoint(1);
  if ( int32 != 1 ) breakpoint(1);


  // Simple patch of 3 bytes
  {
    UINT8 buf[] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20 };
    UINT8 expectedBuf[] = { 0x11, 0x12, 0x23, 0x24, 0x25, 0x16, 0x17, 0x18, 0x19, 0x20 };
    uint32 = ApplyPatch( (UINT8*)"\x13\x14\x15", NULL, 3,   (UINT8*)"\x23\x24\x25", NULL,   buf, 10, 0, 0);
    if ( uint32 != 1 ) breakpoint(1);
    if ( memcmp(buf, expectedBuf, 10) != 0 ) breakpoint(1);
  }
  // Patch 3 bytes with find mask
  {
    UINT8 buf[] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20 };
    UINT8 expectedBuf[] = { 0x11, 0x12, 0x23, 0x24, 0x25, 0x16, 0x17, 0x18, 0x19, 0x20 };
    uint32 = ApplyPatch( (UINT8*)"\x13\x00\x15", (UINT8*)"\xFF\x00\xFF", 3,   (UINT8*)"\x23\x24\x25", NULL,   buf, 10, 0, 0);
    if ( uint32 != 1 ) breakpoint(1);
    if ( memcmp(buf, expectedBuf, 10) != 0 ) breakpoint(1);
  }
  // Patch 3 bytes with find mask and replacemask
  {
    UINT8 buf[] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20 };
    UINT8 expectedBuf[] = { 0x11, 0x12, 0x23, 0x14, 0x25, 0x16, 0x17, 0x18, 0x19, 0x20 };
    uint32 = ApplyPatch( (UINT8*)"\x13\x00\x15", (UINT8*)"\xFF\x00\xFF", 3,   (UINT8*)"\x23\x24\x25", (UINT8*)"\xFF\x00\xFF",   buf, 10, 0, 0);
    if ( uint32 != 1 ) breakpoint(1);
    if ( memcmp(buf, expectedBuf, 10) != 0 ) breakpoint(1);
  }
  // Patch 3 bytes with find mask and replacemask
  {
    UINT8 buf[] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20 };
    UINT8 expectedBuf[] = { 0x11, 0x12, 0x23, 0x24, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20 };
    uint32 = ApplyPatch(
                (UINT8*)"\x13\x00\x15", (UINT8*)"\xFF\x00\xFF", 3,
                (UINT8*)"\x23\x24\x25", (UINT8*)"\xFF\xFF\x00",
                buf, 10, 0, 0);
    if ( uint32 != 1 ) breakpoint(1);
    if ( memcmp(buf, expectedBuf, 10) != 0 ) breakpoint(1);
  }
  // Patch half a byte
  {
    UINT8 buf[] = { 0x11, 0xCC, 0x13 };
    UINT8 expectedBuf[] = { 0x11, 0xC2, 0x13 };
    uint32 = ApplyPatch((UINT8*)"\xC0", (UINT8*)"\xF0", 1,
                       (UINT8*)"\x22", (UINT8*)"\x0F",
                       buf, 3, 0, 0);
    if ( uint32 != 1 ) breakpoint(1);
    if ( memcmp(buf, expectedBuf, 3) != 0 ) breakpoint(1);
  }
  // Patch clever to clover
  {
    UINT8 buf[] = { 0x01, 0x63, 0x6c, 0x65, 0x76, 0x65, 0x72, 0x02 };
    UINT8 expectedBuf[] = { 0x01, 0x63, 0x6c, 0x6f, 0x76, 0x65, 0x72, 0x02 };
    uint32 = ApplyPatch((UINT8*)"\x43\x6c\x65\x76\x65\x72", (UINT8*)"\xDF\xFF\xFF\xFF\xFF\xFF", 6,
                        (UINT8*)"\x43\x6c\x6f\x76\x65\x72", (UINT8*)"\x00\x00\xFF\x00\x00\x00",
                        buf, 8, 0, 0);
    if ( uint32 != 1 ) breakpoint(1);
    if ( memcmp(buf, expectedBuf, 3) != 0 ) breakpoint(1);
  }
  // Patch Clever to clover
  {
    UINT8 buf[] = { 0x01, 0x43, 0x6c, 0x65, 0x76, 0x65, 0x72, 0x02 };
    UINT8 expectedBuf[] = { 0x01, 0x43, 0x6c, 0x6f, 0x76, 0x65, 0x72, 0x02 };
    uint32 = ApplyPatch((UINT8*)"\x43\x6c\x65\x76\x65\x72", (UINT8*)"\xDF\xFF\xFF\xFF\xFF\xFF", 6,
                        (UINT8*)"\x43\x6c\x6f\x76\x65\x72", (UINT8*)"\x00\x00\xFF\x00\x00\x00",
                        buf, 8, 0, 0);
    if ( uint32 != 1 ) breakpoint(1);
    if ( memcmp(buf, expectedBuf, 3) != 0 ) breakpoint(1);
  }

  return 0;
}
