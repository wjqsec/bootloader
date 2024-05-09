/*
 * a class to keep definitions for all theme settings
 */

extern "C" {
#include <Protocol/GraphicsOutput.h>
}

#include "libegint.h"
#include "../refit/screen.h"
#include "../refit/lib.h"

#include "XTheme.h"
#include "nanosvg.h"

#ifndef DEBUG_ALL
#define DEBUG_XICON 1
#else
#define DEBUG_XICON DEBUG_ALL
#endif

#if DEBUG_XICON == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_XICON, __VA_ARGS__)
#endif

CONST LString8 IconsNames[] = {
  "func_about",
  "func_options",
  "func_clover",
  "func_secureboot",
  "func_secureboot_config",
  "func_reset",
  "func_shutdown",
  "func_help",
  "tool_shell", //8
  "tool_part",
  "tool_rescue",
  "pointer",//11
  "vol_internal",
  "vol_external",
  "vol_optical",
  "vol_firewire",
  "vol_clover" ,
  "vol_internal_hfs" , //17
  "vol_internal_apfs",
  "vol_internal_ntfs",
  "vol_internal_ext3" ,
  "vol_recovery",//21
// not used? will be skipped while theme parsing
  "logo",
  "selection_small",
  "selection_big",  //BUILTIN_SELECTION_BIG=24 we keep this numeration
  //os icons
   "os_mac",  //0 + 25
   "os_tiger",
   "os_leo",
   "os_snow",
   "os_lion",
   "os_cougar",
   "os_mav",
   "os_yos",
   "os_cap", //33
   "os_sierra",
   "os_hsierra",
   "os_moja",  //36
   "os_cata",  //37  //there is no reserve for 10.16, next oses should be added to the end of the list
   "os_linux", //13 + 25 = 38
   "os_ubuntu",
   "os_suse",
   "os_freebsd", //16+25 = 41
   "os_freedos",
   "os_win",
   "os_vista",
   "radio_button", //20+25 = 45
   "radio_button_selected",
   "checkbox",  //22+25 = 47
   "checkbox_checked",
   "scrollbar_background", //24 - present here for SVG theme but should be done more common way
   "scrollbar_holder",
   "os_unknown", //51 == ICON_OTHER_OS
   "os_clover",  //52 == ICON_CLOVER
   //other oses will be added below
   "os_bigsur",  //53 == ICON_BIGSUR
   "os_monterey",  //54 == ICON_MONTEREY
   "os_ventura",  //55 == ICON_VENTURA
   "os_sonoma",  //56 == ICON_SONOMA
  ""
};
const INTN IconsNamesSize = sizeof(IconsNames) / sizeof(IconsNames[0]);

//icons class
//if ImageNight is not set then Image should be used
#define DEC_BUILTIN_ICON(id, ico) { \
Empty = EFI_ERROR(Image.FromPNG(ACCESS_EMB_DATA(ico), ACCESS_EMB_SIZE(ico))); \
}

#define DEC_BUILTIN_ICON2(id, ico, dark) { \
Empty = EFI_ERROR(Image.FromPNG(ACCESS_EMB_DATA(ico), ACCESS_EMB_SIZE(ico))); \
ImageNight.FromPNG(ACCESS_EMB_DATA(dark), ACCESS_EMB_SIZE(dark)); \
}


XIcon::XIcon(INTN Index, XBool TakeEmbedded) : Id(Index), Empty(false)
{
  if (Index >= BUILTIN_ICON_FUNC_ABOUT && Index < IconsNamesSize) { //full table
    Name = IconsNames[Index];
  }
  if (TakeEmbedded) {
    GetEmbedded();
  }
}

void XIcon::GetEmbedded()
{
  switch (Id) {
    case BUILTIN_ICON_FUNC_ABOUT:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_ABOUT, emb_func_about, emb_dark_func_about)
      break;
    case BUILTIN_ICON_FUNC_OPTIONS:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_OPTIONS, emb_func_options, emb_dark_func_options)
      break;
    case BUILTIN_ICON_FUNC_CLOVER:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_CLOVER, emb_func_clover, emb_dark_func_clover)
      break;
    case BUILTIN_ICON_FUNC_SECURE_BOOT:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_SECURE_BOOT, emb_func_secureboot, emb_dark_func_secureboot)
      break;
    case BUILTIN_ICON_FUNC_SECURE_BOOT_CONFIG:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_SECURE_BOOT_CONFIG, emb_func_secureboot_config, emb_dark_func_secureboot_config)
      break;
    case BUILTIN_ICON_FUNC_RESET:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_RESET, emb_func_reset, emb_dark_func_reset)
      break;
    case BUILTIN_ICON_FUNC_EXIT:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_EXIT, emb_func_exit, emb_dark_func_exit)
      break;
    case BUILTIN_ICON_FUNC_HELP:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_FUNC_HELP, emb_func_help, emb_dark_func_help)
      break;
    case BUILTIN_ICON_TOOL_SHELL:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_TOOL_SHELL, emb_func_shell, emb_dark_func_shell)
      break;
    case BUILTIN_ICON_BANNER:
      DEC_BUILTIN_ICON2(BUILTIN_ICON_BANNER, emb_logo, emb_dark_logo)
      break;
    case BUILTIN_SELECTION_SMALL:
      DEC_BUILTIN_ICON2(BUILTIN_SELECTION_SMALL, emb_selection_small, emb_dark_selection_small)
      break;
    case BUILTIN_SELECTION_BIG:
      DEC_BUILTIN_ICON2(BUILTIN_SELECTION_BIG, emb_selection_big, emb_dark_selection_big)
      break;
      //next icons have no dark image
    case BUILTIN_ICON_POINTER:
      DEC_BUILTIN_ICON(BUILTIN_ICON_POINTER, emb_pointer)
      break;
    case BUILTIN_ICON_VOL_INTERNAL:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_INTERNAL, emb_vol_internal)
      break;
    case BUILTIN_ICON_VOL_EXTERNAL:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_EXTERNAL, emb_vol_external)
      break;
    case BUILTIN_ICON_VOL_OPTICAL:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_OPTICAL, emb_vol_optical)
      break;
    case BUILTIN_ICON_VOL_BOOTER:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_BOOTER, emb_vol_internal_booter)
      break;
    case BUILTIN_ICON_VOL_INTERNAL_HFS:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_INTERNAL_HFS, emb_vol_internal_hfs)
      break;
    case BUILTIN_ICON_VOL_INTERNAL_APFS:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_INTERNAL_APFS, emb_vol_internal_apfs)
      break;
    case BUILTIN_ICON_VOL_INTERNAL_NTFS:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_INTERNAL_NTFS, emb_vol_internal_ntfs)
      break;
    case BUILTIN_ICON_VOL_INTERNAL_EXT3:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_INTERNAL_EXT3, emb_vol_internal_ext)
      break;
    case BUILTIN_ICON_VOL_INTERNAL_REC:
      DEC_BUILTIN_ICON(BUILTIN_ICON_VOL_INTERNAL_REC, emb_vol_internal_recovery)
      break;
    case BUILTIN_RADIO_BUTTON:
      DEC_BUILTIN_ICON(BUILTIN_RADIO_BUTTON, emb_radio_button)
      break;
    case BUILTIN_RADIO_BUTTON_SELECTED:
      DEC_BUILTIN_ICON(BUILTIN_RADIO_BUTTON_SELECTED, emb_radio_button_selected)
      break;
    case BUILTIN_CHECKBOX:
      DEC_BUILTIN_ICON(BUILTIN_CHECKBOX, emb_checkbox)
      break;
    case BUILTIN_CHECKBOX_CHECKED:
      DEC_BUILTIN_ICON(BUILTIN_CHECKBOX_CHECKED, emb_checkbox_checked)
      break;
    case BUILTIN_ICON_SELECTION:
      Name = "selection_indicator"_XS8;
      DEC_BUILTIN_ICON(BUILTIN_ICON_SELECTION, emb_selection_indicator)
      break;
    default:
      //     Image.setEmpty(); //done by ctor?
      break;
  }
  //something to do else?
}

//copy from XImage for our purpose
EFI_STATUS XIcon::LoadXImage(const EFI_FILE *BaseDir, const char* IconName)
{
  return LoadXImage(BaseDir, XStringW().takeValueFrom(IconName));
}

EFI_STATUS XIcon::LoadXImage(const EFI_FILE *BaseDir, const wchar_t* LIconName)
{
  return LoadXImage(BaseDir, XStringW().takeValueFrom(LIconName));
}
//dont call this procedure for SVG theme BaseDir == NULL?
//it can be used for other files
EFI_STATUS XIcon::LoadXImage(const EFI_FILE *BaseDir, const XStringW& IconName)
{
  EFI_STATUS Status = Image.LoadXImage(BaseDir, IconName);
  ImageNight.LoadXImage(BaseDir, IconName + L"_night"_XSW);
  return Status;
}

const XImage& XIcon::GetBest(XBool night) const
{
  const XImage& RetImage = (night && !ImageNight.isEmpty())? ImageNight : Image;
  return RetImage;
}



