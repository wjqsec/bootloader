/*
 * AcpiPatcher.h
 *
 *  Created on: 16 Apr 2020
 *      Author: jief
 */

#ifndef PLATFORM_ACPIPATCHER_H_
#define PLATFORM_ACPIPATCHER_H_

#include "../Platform/Volume.h"
#include "../Platform/MacOsVersion.h"

#pragma pack(push)
#pragma pack(1)

typedef struct {

  EFI_ACPI_DESCRIPTION_HEADER Header;
  UINT32                      Entry;

} RSDT_TABLE;

typedef struct {

  EFI_ACPI_DESCRIPTION_HEADER Header;
  UINT64                      Entry;

} XSDT_TABLE;

#pragma pack(pop)



extern UINT64                          BiosDsdt;
extern UINT32                          BiosDsdtLen;
#define                                acpi_cpu_max 128
extern UINT8                           acpi_cpu_count;
extern CHAR8                           *acpi_cpu_name[];
extern UINT8                           acpi_cpu_processor_id[];
extern CHAR8                           *acpi_cpu_score;

extern UINT64                    machineSignature;


//ACPI
EFI_STATUS
PatchACPI(IN REFIT_VOLUME *Volume, const MacOsVersion& OSVersion);

EFI_STATUS
PatchACPI_OtherOS(CONST CHAR16* OsSubdir, XBool DropSSDT);

UINT8
Checksum8 (
  void *startPtr,
  UINT32 len
  );

void FixChecksum(EFI_ACPI_DESCRIPTION_HEADER* Table);


void
SaveOemDsdt (
  XBool FullPatch
  );

void
SaveOemTables (void);

EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE
*GetFadt (void);


void
GetAcpiTablesList (void);


#endif /* PLATFORM_ACPIPATCHER_H_ */
