/*
 * DataHubCpu.h
 *
 *  Created on: 16 Apr 2020
 *      Author: jief
 */

#ifndef PLATFORM_DATAHUBCPU_H_
#define PLATFORM_DATAHUBCPU_H_

#include "../gui/menu_items/menu_items.h"
#include "DataHubExt.h"

EFI_STATUS
EFIAPI
SetVariablesForOSX (LOADER_ENTRY *Entry);


EFI_STATUS
EFIAPI
LogDataHub (
  const EFI_GUID& TypeGuid,
  CONST CHAR16   *Name,
  const void     *Data,
  UINT32   DataSize
  );

void
EFIAPI
SetupDataForOSX (XBool Hibernate);


#endif /* PLATFORM_DATAHUBCPU_H_ */
