/*
 * common.h
 *
 *  Created on: 5 Apr 2020
 *      Author: jief
 */

#ifndef ENTRY_SCAN_COMMON_H_
#define ENTRY_SCAN_COMMON_H_

#include "../cpp_foundation/XString.h"

INTN
StrniCmp (
  IN CONST CHAR16 *Str1,
  IN CONST CHAR16 *Str2,
  IN UINTN  Count
  );

CONST CHAR16
*StriStr(
  IN CONST CHAR16 *Str,
  IN CONST CHAR16 *SearchFor
  );

void
StrToLower (
  IN CHAR16 *Str
  );

void AlertMessage (IN const XStringW& Title, IN const XStringW& Message);

XBool
YesNoMessage (
  IN CONST CHAR16 *Title,
  IN CONST CHAR16 *Message);


UINT64 TimeDiff(UINT64 t0, UINT64 t1); //double in Platform.h



#endif /* ENTRY_SCAN_COMMON_H_ */
