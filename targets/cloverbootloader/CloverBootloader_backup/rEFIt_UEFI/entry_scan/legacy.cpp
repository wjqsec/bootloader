/*
 * refit/scan/legacy.c
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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

#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile
#include "entry_scan.h"
#include "../refit/screen.h"
#include "../refit/menu.h"
#include "../gui/REFIT_MENU_SCREEN.h"
#include "../gui/REFIT_MAINMENU_SCREEN.h"
#include "../Platform/Volumes.h"
#include "../libeg/XTheme.h"
#include "../include/OSFlags.h"

#ifndef DEBUG_ALL
#define DEBUG_SCAN_LEGACY 1
#else
#define DEBUG_SCAN_LEGACY DEBUG_ALL
#endif

#if DEBUG_SCAN_LEGACY == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_SCAN_LEGACY, __VA_ARGS__)
#endif

//the function is not in the class and deals always with MainMenu
//I made args as pointers to have an ability to call with NULL
XBool AddLegacyEntry(IN const XStringW& FullTitle, IN const XStringW& _LoaderTitle, IN REFIT_VOLUME *Volume,
    IN const XIcon* Image, IN const XIcon* DriveImage, IN wchar_t Hotkey, IN XBool CustomEntry)
{
  LEGACY_ENTRY      *Entry, *SubEntry;
  REFIT_MENU_SCREEN *SubScreen;
  XStringW           VolDesc;
  wchar_t             ShortcutLetter = 0;
//  INTN               i;
  
DBG("    AddLegacyEntry:\n");
	if (Volume == NULL) {
		DBG("  Volume==0\n");
		return false;
	}
DBG("      FullTitle=%ls\n", FullTitle.wc_str());
DBG("      LoaderTitle=%ls\n", _LoaderTitle.wc_str());
DBG("      Volume->LegacyOS.Name=%ls\n", Volume->LegacyOS.Name.wc_str());

  // Ignore this loader if it's device path is already present in another loader
  for (UINTN i = 0; i < MainMenu.Entries.size(); ++i) {
    REFIT_ABSTRACT_MENU_ENTRY& MainEntry = MainMenu.Entries[i];
//      DBG("entry %lld\n", i);
    // Only want legacy
    if (MainEntry.getLEGACY_ENTRY()) {
      if ( MainEntry.getLEGACY_ENTRY()->DevicePathString.isEqualIC(Volume->DevicePathString) ) {
        return  true;
      }
    }
  }

  XStringW LoaderTitle;
  if ( _LoaderTitle.isEmpty() ) {
    LoaderTitle = Volume->LegacyOS.Name;
  }else{
    LoaderTitle = _LoaderTitle;
  }

  XStringW LTitle;
  if (LoaderTitle.isEmpty()) {
    if (Volume->LegacyOS.Name.notEmpty()) {
      LTitle.takeValueFrom(Volume->LegacyOS.Name);
      if (Volume->LegacyOS.Name[0] == 'W' || Volume->LegacyOS.Name[0] == 'L')
        ShortcutLetter = (wchar_t)Volume->LegacyOS.Name[0]; // cast safe because value is 'W' or 'L'
    } else
      LTitle = L"Legacy OS"_XSW;
  } else
    LTitle = LoaderTitle;
  if (Volume->VolName.notEmpty())
    VolDesc = Volume->VolName;
  else
    VolDesc.takeValueFrom((Volume->DiskKind == DISK_KIND_OPTICAL) ? L"CD" : L"HD");
//DBG("VolDesc=%ls\n", VolDesc);

  // prepare the menu entry
  Entry = new LEGACY_ENTRY;
  if ( FullTitle.notEmpty() ) {
    Entry->Title = FullTitle;
  } else {
    if (ThemeX->BootCampStyle) {
      Entry->Title = LTitle;
    } else {
        Entry->Title.SWPrintf("Boot %ls from %ls", LoaderTitle.wc_str(), VolDesc.wc_str());
    }
  }
  DBG("      Entry->Title=%ls\n", Entry->Title.wc_str());

  Entry->Row          = 0;
  Entry->ShortcutLetter = (Hotkey == 0) ? ShortcutLetter : Hotkey;

  if ( Image && !Image->isEmpty() ) {
    Entry->Image = *Image;
  } else {
    Entry->Image = ThemeX->LoadOSIcon(Volume->LegacyOS.IconName);
    if (Entry->Image.Image.isEmpty()) {
      Entry->Image = ThemeX->GetIcon("os_win"_XS8); //we have no legacy.png
    }
  }

//  DBG("IconName=%ls\n", Volume->LegacyOS.IconName);

    Entry->DriveImage = (DriveImage != NULL) ? *DriveImage : ScanVolumeDefaultIcon(Volume, Volume->LegacyOS.Type, Volume->DevicePath);

  //  DBG("HideBadges=%d Volume=%ls\n", GlobalConfig.HideBadges, Volume->VolName);
  //  DBG("Title=%ls OSName=%ls OSIconName=%ls\n", LoaderTitle, Volume->OSName, Volume->OSIconName);
  //actions
  Entry->AtClick = ActionSelect;
  Entry->AtDoubleClick = ActionEnter;
  Entry->AtRightClick = ActionDetails;
  if (ThemeX->HideBadges & HDBADGES_SHOW) {
    if (ThemeX->HideBadges & HDBADGES_SWAP) { //will be scaled later
      Entry->BadgeImage.Image = XImage(Entry->DriveImage.Image, 0); //ThemeX->BadgeScale/16.f); //0 accepted
    } else {
      Entry->BadgeImage.Image = XImage(Entry->Image.Image, 0); //ThemeX->BadgeScale/16.f);
    }
  }
  Entry->Volume           = Volume;
  Entry->DevicePathString = Volume->DevicePathString;
//  Entry->LoadOptions      = (Volume->DiskKind == DISK_KIND_OPTICAL) ? "CD"_XS8 : ((Volume->DiskKind == DISK_KIND_EXTERNAL) ? "USB"_XS8 : "HD"_XS8);
  Entry->LoadOptions.setEmpty();
  Entry->LoadOptions.Add((Volume->DiskKind == DISK_KIND_OPTICAL) ? "CD" : ((Volume->DiskKind == DISK_KIND_EXTERNAL) ? "USB" : "HD"));
  
  // If this isn't a custom entry make sure it's not hidden by a custom entry
  if (!CustomEntry) {
    for (size_t CustomIndex = 0 ; CustomIndex < GlobalConfig.CustomLegacyEntries.size() ; ++CustomIndex ) {
      CUSTOM_LEGACY_ENTRY& Custom = GlobalConfig.CustomLegacyEntries[CustomIndex];
      if ( Custom.settings.Disabled  ||  OSFLAG_ISSET(Custom.getFlags(), OSFLAG_DISABLED)  ||  Custom.settings.Hidden ) {
        if (Custom.settings.Volume.notEmpty()) {
          if ( !Volume->DevicePathString.contains(Custom.settings.Volume)  &&  !Volume->VolName.contains(Custom.settings.Volume) ) {
            if (Custom.settings.Type != 0) {
              if (Custom.settings.Type == Volume->LegacyOS.Type) {
                Entry->Hidden = true;
              }
            } else {
              Entry->Hidden = true;
            }
          }
        } else if (Custom.settings.Type != 0) {
          if (Custom.settings.Type == Volume->LegacyOS.Type) {
            Entry->Hidden = true;
          }
        }
      }
    }
  }
  
  // create the submenu
  SubScreen = new REFIT_MENU_SCREEN;
//  SubScreen->Title = L"Boot Options for "_XSW + LoaderTitle + L" on "_XSW + VolDesc;
	SubScreen->Title.SWPrintf("Boot Options for %ls on %ls", LoaderTitle.wc_str(), VolDesc.wc_str());

  SubScreen->TitleImage = Entry->Image;  //it is XIcon
  SubScreen->ID = SCREEN_BOOT;
  SubScreen->GetAnime();
  // default entry
  SubEntry =  new LEGACY_ENTRY;
  SubEntry->Title = L"Boot "_XSW + LoaderTitle;
//  SubEntry->Tag           = TAG_LEGACY;
  SubEntry->Volume           = Entry->Volume;
  SubEntry->DevicePathString = Entry->DevicePathString;
  SubEntry->LoadOptions      = Entry->LoadOptions;
  SubEntry->AtClick       = ActionEnter;
  SubScreen->AddMenuEntry(SubEntry, true);
  SubScreen->AddMenuEntry(&MenuEntryReturn, false);
  Entry->SubScreen = SubScreen;
  MainMenu.AddMenuEntry(Entry, true);
//  DBG(" added '%ls' OSType=%d Icon=%ls\n", Entry->Title, Volume->LegacyOS.Type, Volume->LegacyOS.IconName);
  return true;
}

void ScanLegacy(void)
{
  UINTN                   VolumeIndex, VolumeIndex2;
  XBool                    ShowVolume, HideIfOthersFound;
  REFIT_VOLUME            *Volume;
  
  DBG("Scanning legacy ...\n");
  
  for (VolumeIndex = 0; VolumeIndex < Volumes.size(); VolumeIndex++) {
    Volume = &Volumes[VolumeIndex];
    if ((Volume->BootType != BOOTING_BY_PBR) &&
        (Volume->BootType != BOOTING_BY_MBR) &&
        (Volume->BootType != BOOTING_BY_CD)) {
//      DBG(" not legacy\n");
      continue;
    }

    if (((1ull<<Volume->DiskKind) & GlobalConfig.DisableFlags) != 0)
    {
//      DBG(" hidden\n");
      continue;
    }
//    DBG("not hidden\n");
    ShowVolume = false;
    HideIfOthersFound = false;
    if (Volume->IsAppleLegacy) {
      ShowVolume = true;
      HideIfOthersFound = true;
    } else if (Volume->HasBootCode) {
      ShowVolume = true;
      if ((Volume->WholeDiskBlockIO == 0) &&
          Volume->BlockIOOffset == 0 )
        // this is a whole disk (MBR) entry; hide if we have entries for partitions
        HideIfOthersFound = true;
//      DBG("Hide it!\n");
    }
    if (HideIfOthersFound) {
      // check for other bootable entries on the same disk
      //if PBR exists then Hide MBR
      for (VolumeIndex2 = 0; VolumeIndex2 < Volumes.size(); VolumeIndex2++) {
//        DBG("what to hide %d\n", VolumeIndex2);
        if (VolumeIndex2 != VolumeIndex &&
            Volumes[VolumeIndex2].HasBootCode &&
            Volumes[VolumeIndex2].WholeDiskBlockIO == Volume->BlockIO){
          ShowVolume = false;
//         DBG("PBR volume at index %d\n", VolumeIndex2);
          break;
        }
      }
    }
    
    if (ShowVolume && (!Volume->Hidden)){
//      DBG(" add legacy\n");
      if (!AddLegacyEntry(L""_XSW, L""_XSW, Volume, NULL, NULL, 0, false)) {
        DBG("...entry not added\n");
      };
    } else {
      DBG(" hidden\n");
    }
  }
}

// Add custom legacy
void AddCustomLegacy(void)
{
  UINTN                VolumeIndex, VolumeIndex2;
  XBool                ShowVolume, HideIfOthersFound;
  REFIT_VOLUME        *Volume;
  XIcon MainIcon;
  XIcon DriveIcon;
  
//  DBG("Custom legacy start\n");
  if (GlobalConfig.CustomLegacyEntries.notEmpty()) {
    DbgHeader("AddCustomLegacy");
  }

  // Traverse the custom entries
  for (size_t i = 0 ; i < GlobalConfig.CustomLegacyEntries.size() ; ++i ) {
    CUSTOM_LEGACY_ENTRY& Custom = GlobalConfig.CustomLegacyEntries[i];
    if (Custom.settings.Disabled  ||  OSFLAG_ISSET(Custom.getFlags(), OSFLAG_DISABLED)) {
      DBG("Custom legacy %zu skipped because it is disabled.\n", i);
      continue;
    }
//    if (!gSettings.ShowHiddenEntries && OSFLAG_ISSET(Custom.Flags, OSFLAG_HIDDEN)) {
//      DBG("Custom legacy %llu skipped because it is hidden.\n", i);
//      continue;
//    }
    if (Custom.settings.Volume.notEmpty()) {
      DBG("Custom legacy %zu matching \"%ls\" ...\n", i, Custom.settings.Volume.wc_str());
    }
    for (VolumeIndex = 0; VolumeIndex < Volumes.size(); ++VolumeIndex) {
      Volume = &Volumes[VolumeIndex];
      
      DBG("   Checking volume \"%ls\" (%ls) ... ", Volume->VolName.wc_str(), Volume->DevicePathString.wc_str());
      
      // skip volume if its kind is configured as disabled
      if (((1ull<<Volume->DiskKind) & GlobalConfig.DisableFlags) != 0)
      {
        DBG("skipped because media is disabled\n");
        continue;
      }
      
      if (Custom.settings.VolumeType != 0) {
        if (((1ull<<Volume->DiskKind) & Custom.settings.VolumeType) == 0) {
          DBG("skipped because media is ignored (VolumeType=%d DiskKind=%d)\n", Custom.settings.VolumeType, Volume->DiskKind);
          continue;
        }
      }
      if ((Volume->BootType != BOOTING_BY_PBR) &&
          (Volume->BootType != BOOTING_BY_MBR) &&
          (Volume->BootType != BOOTING_BY_CD)) {
        DBG("skipped because volume is not legacy bootable\n");
        continue;
      }
      
      ShowVolume = false;
      HideIfOthersFound = false;
      if (Volume->IsAppleLegacy) {
        ShowVolume = true;
        HideIfOthersFound = true;
      } else if (Volume->HasBootCode) {
        ShowVolume = true;
        if ((Volume->WholeDiskBlockIO == 0) &&
            Volume->BlockIOOffset == 0) {
          // this is a whole disk (MBR) entry; hide if we have entries for partitions
          HideIfOthersFound = true;
        }
      }
      if (HideIfOthersFound) {
        // check for other bootable entries on the same disk
        //if PBR exists then Hide MBR
        for (VolumeIndex2 = 0; VolumeIndex2 < Volumes.size(); VolumeIndex2++) {
          if (VolumeIndex2 != VolumeIndex &&
              Volumes[VolumeIndex2].HasBootCode &&
              Volumes[VolumeIndex2].WholeDiskBlockIO == Volume->BlockIO) {
            ShowVolume = false;
            break;
          }
        }
      }
      
      if ( !ShowVolume ) {
        DBG("skipped because volume ShowVolume==false\n");
        continue;
      }
      if ( Volume->Hidden ) {
        DBG("skipped because volume is hidden\n");
        continue;
      }

      // Check for exact volume matches
      if (Custom.settings.Volume.notEmpty()) {
        if ((StrStr(Volume->DevicePathString.wc_str(), Custom.settings.Volume.wc_str()) == NULL) &&
            ((Volume->VolName.isEmpty()) || (StrStr(Volume->VolName.wc_str(), Custom.settings.Volume.wc_str()) == NULL))) {
          DBG("skipped\n");
          continue;
        }
        // Check if the volume should be of certain os type
        if ((Custom.settings.Type != 0) && (Custom.settings.Type != Volume->LegacyOS.Type)) {
          DBG("skipped because wrong type\n");
          continue;
        }
      } else if ((Custom.settings.Type != 0) && (Custom.settings.Type != Volume->LegacyOS.Type)) {
        DBG("skipped because wrong type\n");
        continue;
      }
      // Change to custom image if needed
      MainIcon = Custom.Image;
      if (MainIcon.Image.isEmpty()) {
        MainIcon.Image.LoadXImage(&ThemeX->getThemeDir(), Custom.settings.ImagePath);
      }

      // Change to custom drive image if needed
      DriveIcon = Custom.DriveImage;
      if (DriveIcon.Image.isEmpty()) {
        DriveIcon.Image.LoadXImage(&ThemeX->getThemeDir(), Custom.settings.DriveImagePath);
      }
      // Create a legacy entry for this volume
      DBG("\n");
      if (AddLegacyEntry(Custom.settings.FullTitle, Custom.settings.Title, Volume, &MainIcon, &DriveIcon, Custom.settings.Hotkey, true))
      {
//        DBG("match!\n");
      }
    }
  }
  //DBG("Custom legacy end\n");
}
