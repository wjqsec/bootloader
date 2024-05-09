/*
 * refit/menu.c
 * Menu functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
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

#include "./REFIT_MENU_SCREEN.h"
#include <Platform.h>
#include "../Platform/BasicIO.h"
#include "../libeg/libegint.h"   //this includes platform.h
//#include "../include/scroll_images.h"

//#include "colors.h"

#include "../libeg/nanosvg.h"
#include "../libeg/FloatLib.h"
#include "../Platform/HdaCodecDump.h"
#include "REFIT_MENU_SCREEN.h"
//#include "screen.h"
#include "../cpp_foundation/XString.h"
#include "../libeg/XTheme.h"
#include "../libeg/VectorGraphics.h" // for testSVG
#include "shared_with_menu.h"
#include "../refit/menu.h"  // for DrawTextXY. Must disappear soon.
#include "../Platform/AcpiPatcher.h"
#include "../Platform/Nvram.h"
#include "../refit/screen.h"
#include "../Platform/Events.h"
#include "../Settings/Self.h"
#include "../Platform/Volumes.h"
#include "../include/OSFlags.h"

#ifndef DEBUG_ALL
#define DEBUG_MENU 1
#else
#define DEBUG_MENU DEBUG_ALL
#endif

#if DEBUG_MENU == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_MENU, __VA_ARGS__)
#endif

#include "../Platform/Settings.h"
#include "../Platform/StartupSound.h" // for audioIo

//
const CHAR16 ArrowUp[2]   = { ARROW_UP, 0 }; //defined in  Simple Text Out protocol
const CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };
//
//XBool MainAnime = false;
//
////TODO Scroll variables must be a part of REFIT_SCREEN
////XBool ScrollEnabled = false;
//
static INTN ScrollbarYMovement;
//
//
////#define TextHeight (FONT_CELL_HEIGHT + TEXT_YMARGIN * 2)
//
//
//
INTN row0Count, row0PosX;
INTN row1Count, row1PosX;
INTN row0PosY;

INTN OldX = 0, OldY = 0;
INTN OldTextWidth = 0, OldTextHeight = 0;
UINTN OldRow = 0;
INTN MenuWidth , TimeoutPosY;
UINTN MenuMaxTextLen = 0;
INTN EntriesPosX, EntriesPosY;

XBool mGuiReady = false;

XPointer REFIT_MENU_SCREEN::mPointer;



void REFIT_MENU_SCREEN::AddMenuInfo_f(CONST char *format, ...)
{

//DBG("%s, %s : Line=%s\n", __FILE__, __LINE__, XString(Line).c);
  REFIT_INFO_DIALOG *InputBootArgs;

  InputBootArgs = new REFIT_INFO_DIALOG;
  VA_LIST va;
	VA_START(va, format);
  InputBootArgs->Title.vSWPrintf(format, va);
  VA_END(va);
  InputBootArgs->AtClick = ActionLight;
  AddMenuEntry(InputBootArgs, true);
}

//
// Graphics helper functions
//

/*
  SelectionImages:
    [0] SelectionBig
    [2] SelectionSmall
    [4] SelectionIndicator
  Buttons:
    [0] radio_button
    [1] radio_button_selected
    [2] checkbox
    [3] checkbox_checked
*/


//
// Scrolling functions
//
#define CONSTRAIN_MIN(Variable, MinValue) if (Variable < MinValue) Variable = MinValue
#define CONSTRAIN_MAX(Variable, MaxValue) if (Variable > MaxValue) Variable = MaxValue

void REFIT_MENU_SCREEN::InitScroll(IN INTN ItemCount, IN UINTN MaxCount,
                       IN UINTN VisibleSpace, IN INTN Selected)
{
  //ItemCount - a number to scroll (Row0)
  //MaxCount - total number (Row0 + Row1)
  //VisibleSpace - a number to fit

  ScrollState.LastSelection = ScrollState.CurrentSelection = Selected;
  //MaxIndex, MaxScroll, MaxVisible are indexes, 0..N-1
  ScrollState.MaxIndex = (INTN)MaxCount - 1;
  ScrollState.MaxScroll = ItemCount - 1;

  if (VisibleSpace == 0) {
    ScrollState.MaxVisible = ScrollState.MaxScroll;
  } else {
    ScrollState.MaxVisible = (INTN)VisibleSpace - 1;
  }

  if (ScrollState.MaxVisible >= ItemCount) {
      ScrollState.MaxVisible = ItemCount - 1;
  }

  ScrollState.MaxFirstVisible = ScrollState.MaxScroll - ScrollState.MaxVisible;
  CONSTRAIN_MIN(ScrollState.MaxFirstVisible, 0);
  ScrollState.FirstVisible = MIN(Selected, ScrollState.MaxFirstVisible);


  ScrollState.IsScrolling = (ScrollState.MaxFirstVisible > 0);
  ScrollState.PaintAll = true;
  ScrollState.PaintSelection = false;

  ScrollState.LastVisible = ScrollState.FirstVisible + ScrollState.MaxVisible;

  //scroll bar geometry
  if (!ThemeX->TypeSVG) {
    UpButton.Width      = ThemeX->ScrollWidth; // 16
    UpButton.Height     = ThemeX->ScrollButtonsHeight; // 20
    DownButton.Width    = UpButton.Width;
    DownButton.Height   = ThemeX->ScrollButtonsHeight;
    BarStart.Height     = ThemeX->ScrollBarDecorationsHeight; // 5
    BarEnd.Height       = ThemeX->ScrollBarDecorationsHeight;
    ScrollStart.Height  = ThemeX->ScrollScrollDecorationsHeight; // 7
    ScrollEnd.Height    = ThemeX->ScrollScrollDecorationsHeight;

  } else {
    UpButton.Width      = ThemeX->ScrollWidth; // 16
    UpButton.Height     = 0; // 20
    DownButton.Width    = UpButton.Width;
    DownButton.Height   = 0;
    BarStart.Height     = ThemeX->ScrollBarDecorationsHeight; // 5
    BarEnd.Height       = ThemeX->ScrollBarDecorationsHeight;
    ScrollStart.Height  = 0; // 7
    ScrollEnd.Height    = 0;
  }

}

void REFIT_MENU_SCREEN::UpdateScroll(IN UINTN Movement)
{
  INTN Lines;
  UINTN ScrollMovement = SCROLL_SCROLL_DOWN;
  ScrollState.LastSelection = ScrollState.CurrentSelection;

  switch (Movement) {
    case SCROLL_SCROLLBAR_MOVE:
      ScrollbarYMovement += ScrollbarNewPointerPlace.YPos - ScrollbarOldPointerPlace.YPos;
      ScrollbarOldPointerPlace.XPos = ScrollbarNewPointerPlace.XPos;
      ScrollbarOldPointerPlace.YPos = ScrollbarNewPointerPlace.YPos;
      Lines = ScrollbarYMovement * ScrollState.MaxIndex / ScrollbarBackground.Height;
      ScrollbarYMovement = ScrollbarYMovement - Lines * (ScrollState.MaxVisible * ThemeX->TextHeight - 16 - 1) / ScrollState.MaxIndex;
      if (Lines < 0) {
        Lines = -Lines;
        ScrollMovement = SCROLL_SCROLL_UP;
      }
      for (INTN i = 0; i < Lines; i++)
        UpdateScroll(ScrollMovement);
      break;

    case SCROLL_LINE_UP: //of left = decrement
      if (ScrollState.CurrentSelection > 0) {
        ScrollState.CurrentSelection --;
        if (ScrollState.CurrentSelection < ScrollState.FirstVisible) {
          ScrollState.PaintAll = true;
          ScrollState.FirstVisible = ScrollState.CurrentSelection;
        }
        if (ScrollState.CurrentSelection == ScrollState.MaxScroll) {
          ScrollState.PaintAll = true;
        }
        if ((ScrollState.CurrentSelection < ScrollState.MaxScroll) &&
             (ScrollState.CurrentSelection > ScrollState.LastVisible)) {
          ScrollState.PaintAll = true;
          ScrollState.LastVisible = ScrollState.CurrentSelection;
          ScrollState.FirstVisible = ScrollState.LastVisible - ScrollState.MaxVisible;
        }
      }
      break;

    case SCROLL_LINE_DOWN: //or right -- increment
      if (ScrollState.CurrentSelection < ScrollState.MaxIndex) {
        ScrollState.CurrentSelection++;
        if ((ScrollState.CurrentSelection > ScrollState.LastVisible) &&
            (ScrollState.CurrentSelection <= ScrollState.MaxScroll)){
          ScrollState.PaintAll = true;
          ScrollState.FirstVisible++;
          CONSTRAIN_MAX(ScrollState.FirstVisible, ScrollState.MaxFirstVisible);
        }
        if (ScrollState.CurrentSelection == ScrollState.MaxScroll + 1) {
          ScrollState.PaintAll = true;
        }
      }
      break;

    case SCROLL_SCROLL_DOWN:
      if (ScrollState.FirstVisible < ScrollState.MaxFirstVisible) {
        if (ScrollState.CurrentSelection == ScrollState.FirstVisible)
          ScrollState.CurrentSelection++;
        ScrollState.FirstVisible++;
        ScrollState.LastVisible++;
        ScrollState.PaintAll = true;
      }
      break;

    case SCROLL_SCROLL_UP:
      if (ScrollState.FirstVisible > 0) {
        if (ScrollState.CurrentSelection == ScrollState.LastVisible)
          ScrollState.CurrentSelection--;
        ScrollState.FirstVisible--;
        ScrollState.LastVisible--;
        ScrollState.PaintAll = true;
      }
      break;

    case SCROLL_PAGE_UP:
      if (ScrollState.CurrentSelection > 0) {
        if (ScrollState.CurrentSelection == ScrollState.MaxIndex) {   // currently at last entry, special treatment
          if (ScrollState.IsScrolling)
            ScrollState.CurrentSelection -= ScrollState.MaxVisible - 1;  // move to second line without scrolling
          else
            ScrollState.CurrentSelection = 0;                // move to first entry
        } else {
          if (ScrollState.FirstVisible > 0)
            ScrollState.PaintAll = true;
          ScrollState.CurrentSelection -= ScrollState.MaxVisible;          // move one page and scroll synchronously
          ScrollState.FirstVisible -= ScrollState.MaxVisible;
        }
        CONSTRAIN_MIN(ScrollState.CurrentSelection, 0);
        CONSTRAIN_MIN(ScrollState.FirstVisible, 0);
        if (ScrollState.CurrentSelection < ScrollState.FirstVisible) {
          ScrollState.PaintAll = true;
          ScrollState.FirstVisible = ScrollState.CurrentSelection;
        }
      }
      break;

    case SCROLL_PAGE_DOWN:
//    DBG("cur=%lld, maxInd=%lld, maxVis=%lld, First=%lld, maxFirst=%lld, lastVis=%lld, maxscr=%lld\n",
//        ScrollState.CurrentSelection, ScrollState.MaxIndex, ScrollState.MaxVisible, ScrollState.FirstVisible,
//        ScrollState.MaxFirstVisible, ScrollState.CurrentSelection, ScrollState.LastVisible);

      if (ScrollState.CurrentSelection < ScrollState.MaxIndex) {
        if (ScrollState.CurrentSelection == 0) {   // currently at first entry, special treatment
          if (ScrollState.IsScrolling)
            ScrollState.CurrentSelection = ScrollState.MaxVisible - 1;  // move to second-to-last line without scrolling
          else
            ScrollState.CurrentSelection = ScrollState.MaxIndex;         // move to last entry
        } else {
//          if (ScrollState.FirstVisible < ScrollState.MaxFirstVisible)
//            ScrollState.PaintAll = true;
          ScrollState.CurrentSelection += ScrollState.MaxVisible;          // move one page and scroll synchronously
          ScrollState.FirstVisible += ScrollState.MaxVisible;
        }
        CONSTRAIN_MAX(ScrollState.CurrentSelection, ScrollState.MaxIndex); // if (v>m) v=m;
        CONSTRAIN_MAX(ScrollState.FirstVisible, ScrollState.MaxFirstVisible);
        if ((ScrollState.CurrentSelection > ScrollState.LastVisible) &&
            (ScrollState.CurrentSelection <= ScrollState.MaxScroll)){
 //         ScrollState.PaintAll = true;
          ScrollState.FirstVisible+= ScrollState.MaxVisible;
          CONSTRAIN_MAX(ScrollState.FirstVisible, ScrollState.MaxFirstVisible);
        }
        ScrollState.PaintAll = true;
      }
//    DBG("after cur=%lld, maxInd=%lld, maxVis=%lld, First=%lld, maxFirst=%lld, lastVis=%lld, maxscr=%lld\n",
//        ScrollState.CurrentSelection, ScrollState.MaxIndex, ScrollState.MaxVisible, ScrollState.FirstVisible,
//        ScrollState.MaxFirstVisible, ScrollState.CurrentSelection, ScrollState.LastVisible);

      break;

    case SCROLL_FIRST:
      if (ScrollState.CurrentSelection > 0) {
        ScrollState.CurrentSelection = 0;
        if (ScrollState.FirstVisible > 0) {
          ScrollState.PaintAll = true;
          ScrollState.FirstVisible = 0;
        }
      }
      break;

    case SCROLL_LAST:
      if (ScrollState.CurrentSelection < ScrollState.MaxIndex) {
        ScrollState.CurrentSelection = ScrollState.MaxIndex;
        if (ScrollState.FirstVisible < ScrollState.MaxFirstVisible) {
          ScrollState.PaintAll = true;
          ScrollState.FirstVisible = ScrollState.MaxFirstVisible;
        }
      }
      break;

    case SCROLL_NONE:
      // The caller has already updated CurrentSelection, but we may
      // have to scroll to make it visible.
      if (ScrollState.CurrentSelection < ScrollState.FirstVisible) {
        ScrollState.PaintAll = true;
        ScrollState.FirstVisible = ScrollState.CurrentSelection; // - (ScrollState.MaxVisible >> 1);
        CONSTRAIN_MIN(ScrollState.FirstVisible, 0);
      } else if ((ScrollState.CurrentSelection > ScrollState.LastVisible) &&
                 (ScrollState.CurrentSelection <= ScrollState.MaxScroll)) {
        ScrollState.PaintAll = true;
        ScrollState.FirstVisible = ScrollState.CurrentSelection - ScrollState.MaxVisible;
        CONSTRAIN_MAX(ScrollState.FirstVisible, ScrollState.MaxFirstVisible);
      }
      break;

  }

  if (!ScrollState.PaintAll && ScrollState.CurrentSelection != ScrollState.LastSelection)
    ScrollState.PaintSelection = true;
  ScrollState.LastVisible = ScrollState.FirstVisible + ScrollState.MaxVisible;

  //ycr.ru
  if ((ScrollState.PaintAll) && (Movement != SCROLL_NONE))
    HidePointer();
}

void REFIT_MENU_SCREEN::HidePointer()
{
  if ( mPointer.isAlive() ) mPointer.Hide();
}

EFI_STATUS REFIT_MENU_SCREEN::MouseBirth()
{

  //if ( !mPointer ) mPointer = new XPointer;
  return mPointer.MouseBirth();
}

void REFIT_MENU_SCREEN::KillMouse()
{
  /*if ( mPointer ) */mPointer.KillMouse();
}

void REFIT_MENU_SCREEN::AddMenuInfoLine_f(CONST char *format, ...)
{
  XStringW* s = new XStringW;
  VA_LIST va;
	VA_START(va, format);
  s->vSWPrintf(format, va);
  VA_END(va);
  InfoLines.AddReference(s, true);
}

void REFIT_MENU_SCREEN::AddMenuEntry(IN REFIT_ABSTRACT_MENU_ENTRY *Entry, XBool freeIt)
{
	if ( !Entry ) return;
	Entries.AddReference(Entry, freeIt);
}

// This is supposed to be a destructor ?
void REFIT_MENU_SCREEN::FreeMenu()
{
  REFIT_ABSTRACT_MENU_ENTRY *Tentry = NULL;
//TODO - here we must Free for a list of Entries, Screens, InputBootArgs
  if (Entries.size() > 0) {
    for (UINTN i = 0; i < Entries.size(); i++) {
      Tentry = &Entries[i];
      if (Tentry->SubScreen) {
        // don't free image because of reusing them
        Tentry->SubScreen->FreeMenu();
        Tentry->SubScreen = NULL;
      }
    }
    Entries.setEmpty();
  }
  InfoLines.setEmpty();
}

INTN REFIT_MENU_SCREEN::FindMenuShortcutEntry(IN wchar_t Shortcut)
{
  if (Shortcut >= L'a' && Shortcut <= L'z')
    Shortcut -= (L'a' - L'A');
  if (Shortcut) {
    for (UINTN i = 0; i < Entries.size(); i++) {
      if (Entries[i].ShortcutDigit == Shortcut ||
          Entries[i].ShortcutLetter == Shortcut) {
//        DBG("found entry %lld because shorcut=%x and ShortcutLetter=%x\n", i, Shortcut, Entries[i].ShortcutLetter);
        return i;
      }
    }
  }
  return -1;
}

//
// generic input menu function
// usr-sse2
//
UINTN REFIT_MENU_SCREEN::InputDialog()
{
  //it's impossible so no matter what to do
	if ( !Entries[ScrollState.CurrentSelection].getREFIT_MENU_ITEM_IEM_ABSTRACT() ) {
		DebugLog(2, "BUG : InputDialog called with !Entries[ScrollState.CurrentSelection].REFIT_MENU_ENTRY_ITEM_ABSTRACT()\n");
		return 0; // is it the best thing to do ? CpuDeadLog ?
	}

  EFI_STATUS    Status;
  EFI_INPUT_KEY key;
  UINTN         ind = 0;
  UINTN         MenuExit = 0;
  //UINTN         LogSize;
  UINTN         Pos = (Entries[ScrollState.CurrentSelection]).Row;
  REFIT_MENU_ENTRY_ITEM_ABSTRACT& selectedEntry = *Entries[ScrollState.CurrentSelection].getREFIT_MENU_ITEM_IEM_ABSTRACT();
  INPUT_ITEM    *Item = selectedEntry.Item;
  XStringW      Backup = Item->SValue;
  UINTN         BackupPos, BackupShift;
//  XStringW      Buffer;
  //SCROLL_STATE  StateLine;

  /*
    I would like to see a LineSize that depends on the Title width and the menu width so
    the edit dialog does not extend beyond the menu width.
    There are 3 cases:
    1) Text menu where MenuWidth is min of ConWidth - 6 and max of 50 and all StrLen(Title)
    2) Graphics menu where MenuWidth is measured in pixels and font is fixed width.
       The following works well in my case but depends on font width and minimum screen size.
         LineSize = 76 - StrLen(Screen->Entries[State->CurrentSelection].Title);
    3) Graphics menu where font is proportional. In this case LineSize would depend on the
       current width of the displayed string which would need to be recalculated after
       every change.
    Anyway, the above will not be implemented for now, and LineSize will remain at 38
    because it works.
  */
  UINTN         LineSize = 38;
  // make sure that LineSize is not too big
  UINTN MaxPossibleLineSize = (MenuWidth - selectedEntry.Place.Width) / (INTN)(ThemeX->CharWidth * ThemeX->Scale) - 1;
  if (!ThemeX->TypeSVG && !ThemeX->Proportional && LineSize > MaxPossibleLineSize) {
    LineSize = MaxPossibleLineSize;
  }

#define DBG_INPUTDIALOG 0
#if DBG_INPUTDIALOG
  UINTN         Iteration = 0;
#endif


  BackupShift = Item->LineShift;
  BackupPos = Pos;

  do {

    if (Item->ItemType == BoolValue) {
      Item->BValue = !Item->BValue;
      MenuExit = MENU_EXIT_ENTER;
    } else if (Item->ItemType == RadioSwitch) {
      if (Item->IValue == 3) {
        OldChosenTheme = Pos? Pos - 1: 0xFFFF;
      } else if (Item->IValue == 65) {
        OldChosenSmbios = Pos;
      } else if (Item->IValue == 90) {
        OldChosenConfig = Pos;
      } else if (Item->IValue == 116) {
        OldChosenDsdt = Pos? Pos - 1: 0xFFFF;
      } else if (Item->IValue == 119) {
        OldChosenAudio = Pos;
      }
      MenuExit = MENU_EXIT_ENTER;
    } else if (Item->ItemType == CheckBit) {
      Item->IValue ^= Pos;
      MenuExit = MENU_EXIT_ENTER;
    } else {

      Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);

      if (Status == EFI_NOT_READY) {
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &ind);
        continue;
      }

      switch (key.ScanCode) {
        case SCAN_RIGHT:
          if (Pos + Item->LineShift < Item->SValue.length()) {
            if (Pos < LineSize)
              Pos++;
            else
              Item->LineShift++;
          }
          break;
        case SCAN_LEFT:
          if (Pos > 0)
            Pos--;
          else if (Item->LineShift > 0)
            Item->LineShift--;
          break;
        case SCAN_HOME:
          Pos = 0;
          Item->LineShift=0;
          break;
        case SCAN_END:
          if (Item->SValue.length() < LineSize)
            Pos = Item->SValue.length();
          else {
            Pos = LineSize;
            Item->LineShift = Item->SValue.length() - LineSize;
          }
          break;
        case SCAN_ESC:
          MenuExit = MENU_EXIT_ESCAPE;
          continue;
          break;
        case SCAN_F2:
          SavePreBootLog = true;
          break;
          //not used here
/*        case SCAN_F6:
          Status = egSaveFile(&self.getSelfRootDir(), VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
          if (EFI_ERROR(Status)) {
            Status = egSaveFile(NULL, VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
          }
          break; */
        case SCAN_F10:
          egScreenShot();
          break;

        case SCAN_DELETE:
          // forward delete
          if (Pos + Item->LineShift < Item->SValue.length()) {
            Item->SValue.deleteCharsAtPos(Pos + Item->LineShift);
//            for (i = Pos + Item->LineShift; i < Item->SValue.length(); i++) {
//               Buffer[i] = Buffer[i+1];
//            }
            /*
            // Commented this out because it looks weird - Forward Delete should not
            // affect anything left of the cursor even if it's just to shift more of the
            // string into view.
            if (Item->LineShift > 0 && Item->LineShift + LineSize > StrLen(Buffer)) {
              Item->LineShift--;
              Pos++;
            }
            */
          }
          break;
      }

      switch (key.UnicodeChar) {
        case CHAR_BACKSPACE:
          if (Item->SValue[0] != CHAR_NULL && Pos != 0) {
            Item->SValue.deleteCharsAtPos(Pos + Item->LineShift - 1);
//            for (i = Pos + Item->LineShift; i <= Item->SValue.length(); i++) {
//               Buffer[i-1] = Buffer[i];
//            }
            Item->LineShift > 0 ? Item->LineShift-- : Pos--;
          }

          break;

        case CHAR_LINEFEED:
        case CHAR_CARRIAGE_RETURN:
          MenuExit = MENU_EXIT_ENTER;
          Pos = 0;
          Item->LineShift = 0;
          break;
        default:
          if ((key.UnicodeChar >= 0x20) &&
              (key.UnicodeChar < 0x80)){
            if ( Item->SValue.length() < SVALUE_MAX_SIZE) {
              Item->SValue.insertAtPos(key.UnicodeChar, Pos + Item->LineShift);
//              for (i = StrLen(Buffer)+1; i > Pos + Item->LineShift; i--) {
//                 Buffer[i] = Buffer[i-1];
//              }
//              Buffer[i] = key.UnicodeChar;
              Pos < LineSize ? Pos++ : Item->LineShift++;
            }
          }
          break;
      }
    }
    // Redraw the field
    (Entries[ScrollState.CurrentSelection]).Row = Pos;
    call_MENU_FUNCTION_PAINT_SELECTION(NULL);
  } while (!MenuExit);

  switch (MenuExit) {
    case MENU_EXIT_ENTER:
      Item->Valid = true;
      ApplyInputs();
      break;

    case MENU_EXIT_ESCAPE:
      if ( !Item->SValue.isEqual(Backup) ) {
        Item->SValue = Backup;
        if (Item->ItemType != BoolValue) {
          Item->LineShift = BackupShift;
          (Entries[ScrollState.CurrentSelection]).Row = BackupPos;
        }
        call_MENU_FUNCTION_PAINT_SELECTION(NULL);
      }
      break;
  }
  Item->Valid = false;
  if (Item->SValue.notEmpty()) {
    MsgLog("EDITED: %ls\n", Item->SValue.wc_str());
  }
  return 0;
}


// TimeoutDefault for a wait in seconds
// return EFI_TIMEOUT if no inputs
//the function must be in menu_screen class
//so UpdatePointer(); => mPointer.Update(&gItemID, &Screen->mAction);
EFI_STATUS REFIT_MENU_SCREEN::WaitForInputEventPoll(UINTN TimeoutDefault)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN TimeoutRemain = TimeoutDefault * 100;

  while (TimeoutRemain != 0) {
    Status = WaitFor2EventWithTsc(gST->ConIn->WaitForKey, NULL, 10);
    if (Status != EFI_TIMEOUT) {
      break;
    }
    UpdateFilm();
    if (gSettings.GUI.PlayAsync) {
      CheckSyncSound(false);
    }
    TimeoutRemain--;
    if (mPointer.isAlive()) {
      mPointer.UpdatePointer(!Daylight);
      Status = CheckMouseEvent(); //out: mItemID, mAction
      if (Status != EFI_TIMEOUT) { //this check should return timeout if no mouse events occured
        break;
      }
    }
  }
  return Status;
}


UINTN REFIT_MENU_SCREEN::RunGenericMenu(IN OUT INTN *DefaultEntryIndex, OUT REFIT_ABSTRACT_MENU_ENTRY **ChosenEntry)
{
  EFI_STATUS    Status;
  EFI_INPUT_KEY key;
  //    UINTN         Index;
  INTN          ShortcutEntry;
  XBool         HaveTimeout = false;
  INTN          TimeoutCountdown = 0;
  UINTN         MenuExit;

  if (ChosenEntry == NULL) {
    TextStyle = 0;
  } else {
    TextStyle = 2;
  }
  if (ThemeX->TypeSVG) {
    if (!ThemeX->getTextFace(TextStyle).valid) {
      if (ThemeX->getTextFace(0).valid) {
        TextStyle = 0;
      } else if (ThemeX->getTextFace(2).valid) {
        TextStyle = 2;
      } else if (ThemeX->getTextFace(1).valid) {
        TextStyle = 1;
      } else {
        DBG("no valid text style\n");
//        ThemeX->getTextFace(TextStyle).size = ThemeX->TextHeight - 4;
      }
    }
    if (ThemeX->getTextFace(TextStyle).valid) {
      // TextHeight = (int)((ThemeX->getTextFace(TextStyle].size + 4) * GlobalConfig.Scale);
      //clovy - row height / text size factor
      ThemeX->TextHeight = (int)((ThemeX->getTextFace(TextStyle).size * RowHeightFromTextHeight) * ThemeX->Scale);
    }
  }

  //no default - no timeout!
  if ((*DefaultEntryIndex != -1) && (TimeoutSeconds > 0)) {
    //      DBG("have timeout\n");
    HaveTimeout = true;
    TimeoutCountdown =  TimeoutSeconds;
  }
  MenuExit = 0;

  call_MENU_FUNCTION_INIT(NULL);
  //  DBG("scroll inited\n");
  // override the starting selection with the default index, if any
  if (*DefaultEntryIndex >= 0 && *DefaultEntryIndex <=  ScrollState.MaxIndex) {
     ScrollState.CurrentSelection = *DefaultEntryIndex;
     UpdateScroll(SCROLL_NONE);
  }
  IsDragging = false;
  //  DBG("RunGenericMenu CurrentSelection=%d MenuExit=%d\n",
  //      State.CurrentSelection, MenuExit);

  // exhaust key buffer and be sure no key is pressed to prevent option selection
  // when coming with a key press from timeout=0, for example
  while (ReadAllKeyStrokes()) gBS->Stall(500 * 1000);
  while (!MenuExit)
  {
    // do this BEFORE calling StyleFunc.
    EFI_TIME          Now;
    gRT->GetTime(&Now, NULL);
    if (gSettings.GUI.Timezone != 0xFF) {
      INT32 NowHour = Now.Hour + gSettings.GUI.Timezone;
      if (NowHour <  0 ) NowHour += 24;
      if (NowHour >= 24 ) NowHour -= 24;
      Daylight = (NowHour > 8) && (NowHour < 20);  //this is the screen member
    } else {
      Daylight = true;
    }


    // update the screen
    if (ScrollState.PaintAll) {
      call_MENU_FUNCTION_PAINT_ALL(NULL);
      ScrollState.PaintAll = false;
    } else if (ScrollState.PaintSelection) {
      call_MENU_FUNCTION_PAINT_SELECTION(NULL);
      ScrollState.PaintSelection = false;
    }

    if (HaveTimeout) {
      XStringW TOMessage = SWPrintf("%ls in %lld seconds", TimeoutText.wc_str(), TimeoutCountdown);
      call_MENU_FUNCTION_PAINT_TIMEOUT(TOMessage.wc_str());
    }

    if (gEvent) { //for now used at CD eject.
      MenuExit = MENU_EXIT_ESCAPE;
      ScrollState.PaintAll = true;
      gEvent = 0; //to prevent looping
      break;
    }
    key.UnicodeChar = 0;
    key.ScanCode = 0;
    if (!mGuiReady) {
      mGuiReady = true;
      DBG("GUI ready\n");
    }
    
    Status = WaitForInputEventPoll(1); //wait for 1 seconds.
    if (Status == EFI_TIMEOUT) {
      if (HaveTimeout) {
        if (TimeoutCountdown <= 0) {
          // timeout expired
          MenuExit = MENU_EXIT_TIMEOUT;
          break;
        } else {
          //     gBS->Stall(100000);
          TimeoutCountdown--;
        }
      }
      continue;
    }

    switch (mAction) {
      case ActionSelect:
        ScrollState.LastSelection = ScrollState.CurrentSelection;
        ScrollState.CurrentSelection = mItemID;
        ScrollState.PaintAll = true;
        HidePointer();
        break;
      case ActionEnter:
        ScrollState.LastSelection = ScrollState.CurrentSelection;
        ScrollState.CurrentSelection = mItemID;
        if ( Entries[mItemID].getREFIT_INPUT_DIALOG() ||  Entries[mItemID].getREFIT_MENU_CHECKBIT() ) {
          MenuExit = InputDialog();
        } else if (Entries[mItemID].getREFIT_MENU_SWITCH()) {
          MenuExit = InputDialog();
          ScrollState.PaintAll = true;
          HidePointer();
        } else if (!Entries[mItemID].getREFIT_INFO_DIALOG()) {
          MenuExit = MENU_EXIT_ENTER;
        }
        break;
      case ActionHelp:
        MenuExit = MENU_EXIT_HELP;
        break;
      case ActionOptions:
        ScrollState.LastSelection = ScrollState.CurrentSelection;
        ScrollState.CurrentSelection = mItemID;
        MenuExit = MENU_EXIT_OPTIONS;
        break;
      case ActionDetails:
        ScrollState.LastSelection = ScrollState.CurrentSelection;
        // Index = State.CurrentSelection;
        ScrollState.CurrentSelection = mItemID;
        if ((Entries[mItemID].getREFIT_INPUT_DIALOG()) ||
            (Entries[mItemID].getREFIT_MENU_CHECKBIT())) {
          MenuExit = InputDialog();
        } else if (Entries[mItemID].getREFIT_MENU_SWITCH()) {
          MenuExit = InputDialog();
          ScrollState.PaintAll = true;
          HidePointer();
        } else if (!Entries[mItemID].getREFIT_INFO_DIALOG()) {
          MenuExit = MENU_EXIT_DETAILS;
        }
        break;
      case ActionDeselect:
        ScrollState.LastSelection = ScrollState.CurrentSelection;
        ScrollState.PaintAll = true;
        HidePointer();
        break;
      case ActionFinish:
        MenuExit = MENU_EXIT_ESCAPE;
        break;
      case ActionScrollDown:
        UpdateScroll(SCROLL_SCROLL_DOWN);
        break;
      case ActionScrollUp:
        UpdateScroll(SCROLL_SCROLL_UP);
        break;
      case ActionMoveScrollbar:
        UpdateScroll(SCROLL_SCROLLBAR_MOVE);
        break;
      default:
        break;
    }

    // read key press (and wait for it if applicable)
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);
    if ((Status == EFI_NOT_READY) && (mAction == ActionNone)) {
      continue;
    }
    if (mAction == ActionNone) {
      ReadAllKeyStrokes(); //clean to avoid doubles
    }
    if (HaveTimeout) {
      // the user pressed a key, cancel the timeout
      call_MENU_FUNCTION_PAINT_TIMEOUT(L"");
      HidePointer(); //ycr.ru
      HaveTimeout = false;
    }
//    DBG("key: scancode=0x%x unicode=0x%x\n", key.ScanCode, key.UnicodeChar);
    mAction = ActionNone; //do action once
    // react to key press
    switch (key.ScanCode) {
      case SCAN_UP:
      case SCAN_LEFT:
        UpdateScroll(SCROLL_LINE_UP);
        break;
      case SCAN_DOWN:
      case SCAN_RIGHT:
        UpdateScroll(SCROLL_LINE_DOWN);
        break;
      case SCAN_HOME:
        UpdateScroll(SCROLL_FIRST);
        break;
      case SCAN_END:
        UpdateScroll(SCROLL_LAST);
        break;
      case SCAN_PAGE_UP:
        UpdateScroll(SCROLL_PAGE_UP);
    //    SetNextScreenMode(1);
    //    call_MENU_FUNCTION_INIT(NULL);
        break;
      case SCAN_PAGE_DOWN:
        UpdateScroll(SCROLL_PAGE_DOWN);
     //   SetNextScreenMode(-1);
     //   call_MENU_FUNCTION_INIT(NULL);
        break;
      case SCAN_ESC:
        MenuExit = MENU_EXIT_ESCAPE;
        break;
      case SCAN_INSERT:
        MenuExit = MENU_EXIT_OPTIONS;
        break;

      case SCAN_F1:
        MenuExit = MENU_EXIT_HELP;
        break;
      case SCAN_F2:
        SavePreBootLog = true;
        //let it be twice
        Status = SaveBooterLog(&self.getCloverDir(), PREBOOT_LOG);
// Jief : don't write outside SelfDir
//        if (EFI_ERROR(Status)) {
//          Status = SaveBooterLog(NULL, PREBOOT_LOG);
//        }
        break;
      case SCAN_F3:
         MenuExit = MENU_EXIT_HIDE_TOGGLE;
         break;
      case SCAN_F4:
        SaveOemTables();
        break;
      case SCAN_F5:
        SaveOemDsdt(true); //full patch
        break;
      case SCAN_F6:
        Status = egSaveFile(&self.getCloverDir(), VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
        // Jief : don't write outside SelfDir
//        if (EFI_ERROR(Status)) {
//          Status = egSaveFile(NULL, VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
//        }
        break;
/* just a sample code
      case SCAN_F7:
        Status = egMkDir(&self.getCloverDir(),  L"new_folder");
        DBG("create folder %s\n", efiStrError(Status));
        if (!EFI_ERROR(Status)) {
          Status = egSaveFile(&self.getCloverDir(),  L"new_folder\\new_file.txt", (UINT8*)SomeText, sizeof(*SomeText)+1);
          DBG("create file %s\n", efiStrError(Status));
        }
        break;
*/
      case SCAN_F7:
//      DBG("scan_f7\n");
        if (AudioList.isEmpty()) break;
//      DBG("not empty\n");
        if (OldChosenAudio >= AudioList.size()) {
              OldChosenAudio = 0; //security correction
        }
        SaveHdaDumpBin();
        SaveHdaDumpTxt();

//       DBG("OldChosenAudio=%llu\n", OldChosenAudio);
          Status = gBS->HandleProtocol(AudioList[OldChosenAudio].Handle, &gEfiAudioIoProtocolGuid, (void**)&AudioIo);
          DBG("open %llu audio handle status=%s\n", OldChosenAudio, efiStrError(Status));
          if (!EFI_ERROR(Status)) {
            StartupSoundPlay(&self.getCloverDir(), NULL); //play embedded sound
          }
        
        break;
      case SCAN_F8:
        testSVG();
//        SaveHdaDumpBin();
//        SaveHdaDumpTxt();
        break;

      case SCAN_F9:
        SetNextScreenMode(1);
        egGetScreenSize(&UGAWidth, &UGAHeight); //before init theme
        GlobalConfig.gThemeChanged = true;
        MenuExit = MENU_EXIT_ESCAPE; //to redraw screen
        break;
      case SCAN_F10:
        egScreenShot();
        break;
      case SCAN_F11:
        ResetNvram();
        MenuExit = MENU_EXIT_ESCAPE; //to redraw screen
        break;
      case SCAN_F12:
        MenuExit = MENU_EXIT_EJECT;
        ScrollState.PaintAll = true;
        break;

    }
    switch (key.UnicodeChar) {
      case CHAR_LINEFEED:
      case CHAR_CARRIAGE_RETURN:
        if ((Entries[ScrollState.CurrentSelection].getREFIT_INPUT_DIALOG()) ||
            (Entries[ScrollState.CurrentSelection].getREFIT_MENU_CHECKBIT())) {
          MenuExit = InputDialog();
        } else if (Entries[ScrollState.CurrentSelection].getREFIT_MENU_SWITCH()){
          MenuExit = InputDialog();
          ScrollState.PaintAll = true;
        } else if (Entries[ScrollState.CurrentSelection].getREFIT_MENU_ENTRY_CLOVER()){
          MenuExit = MENU_EXIT_DETAILS;
        } else if (!Entries[ScrollState.CurrentSelection].getREFIT_INFO_DIALOG()) {
          MenuExit = MENU_EXIT_ENTER;
        }
        break;
      case ' ': //CHAR_SPACE
        if ((Entries[ScrollState.CurrentSelection].getREFIT_INPUT_DIALOG()) ||
            (Entries[ScrollState.CurrentSelection].getREFIT_MENU_CHECKBIT())) {
          MenuExit = InputDialog();
        } else if (Entries[ScrollState.CurrentSelection].getREFIT_MENU_SWITCH()){
          MenuExit = InputDialog();
          ScrollState.PaintAll = true;
          HidePointer();
        } else if (!Entries[ScrollState.CurrentSelection].getREFIT_INFO_DIALOG()) {
          MenuExit = MENU_EXIT_DETAILS;
        }
        break;

      default:
        ShortcutEntry = FindMenuShortcutEntry(key.UnicodeChar);
        if (ShortcutEntry >= 0) {
          ScrollState.CurrentSelection = ShortcutEntry;
          MenuExit = MENU_EXIT_ENTER;
        }
        break;
    }
  }

  call_MENU_FUNCTION_CLEANUP(NULL);

	if (ChosenEntry) {
    *ChosenEntry = &Entries[ScrollState.CurrentSelection];
	}

  *DefaultEntryIndex = ScrollState.CurrentSelection;

  return MenuExit;
}

/**
 * Text Mode menu.
 */
void REFIT_MENU_SCREEN::TextMenuStyle(IN UINTN Function, IN CONST CHAR16 *ParamText)
{
  INTN i = 0, j = 0;
  static UINTN TextMenuWidth = 0,ItemWidth = 0, MenuHeight = 0;
  static UINTN MenuPosY = 0;
  //static CHAR16 **DisplayStrings;
  XStringW ResultString;
	UINTN OldChosenItem = ~(UINTN)0;

  switch (Function) {

    case MENU_FUNCTION_INIT:
      // vertical layout
      MenuPosY = 4;

	  if (InfoLines.size() > 0) {
        MenuPosY += InfoLines.size() + 1;
	  }

      MenuHeight = ConHeight - MenuPosY;

	  if (TimeoutSeconds > 0) {
        MenuHeight -= 2;
	  }

      InitScroll(Entries.size(), Entries.size(), MenuHeight, 0);

      // determine width of the menu
      TextMenuWidth = 50;  // minimum
      for (i = 0; i <= ScrollState.MaxIndex; i++) {
        ItemWidth = Entries[i].Title.length();

		  if (TextMenuWidth < ItemWidth) {
          TextMenuWidth = ItemWidth;
      }
    }

	  if (TextMenuWidth > ConWidth - 6) {
        TextMenuWidth = ConWidth - 6;
	  }

    if (Entries[0].getREFIT_MENU_SWITCH() && Entries[0].getREFIT_MENU_SWITCH()->Item->IValue == 90) {
      j = OldChosenConfig;
    } else if (Entries[0].getREFIT_MENU_SWITCH() && Entries[0].getREFIT_MENU_SWITCH()->Item->IValue == 65) {
      j = OldChosenSmbios;
    } else if (Entries[0].getREFIT_MENU_SWITCH() && Entries[0].getREFIT_MENU_SWITCH()->Item->IValue == 116) {
      j = OldChosenDsdt;
    } else if (Entries[0].getREFIT_MENU_SWITCH() && Entries[0].getREFIT_MENU_SWITCH()->Item->IValue == 119) {
      j = OldChosenAudio;
    }

    break;

    case MENU_FUNCTION_CLEANUP:
      // release temporary memory

			// reset default output colours
	  gST->ConOut->SetAttribute(gST->ConOut, ATTR_BANNER);

      break;

    case MENU_FUNCTION_PAINT_ALL:
        // paint the whole screen (initially and after scrolling)
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_BASIC);
      for (i = 0; i < (INTN)ConHeight - 4; i++) {
        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 4 + i);
        gST->ConOut->OutputString(gST->ConOut, BlankLine);
      }

      BeginTextScreen(Title.wc_str());

      if (InfoLines.size() > 0) {
        gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);

        for (i = 0; i < (INTN)InfoLines.size(); i++) {
          gST->ConOut->SetCursorPosition (gST->ConOut, 3, 4 + i);
          gST->ConOut->OutputString (gST->ConOut, InfoLines[i].wc_str());
        }
      }

      for (i = ScrollState.FirstVisible; i <= ScrollState.LastVisible && i <= ScrollState.MaxIndex; i++) {
        gST->ConOut->SetCursorPosition (gST->ConOut, 2, MenuPosY + (i - ScrollState.FirstVisible));

        if (i == ScrollState.CurrentSelection) {
          gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_CURRENT);
        } else {
          gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_BASIC);
        }

        ResultString.takeValueFrom(Entries[i].Title);

        if (Entries[i].getREFIT_INPUT_DIALOG()) {
          REFIT_INPUT_DIALOG& entry = (REFIT_INPUT_DIALOG&) Entries[i];
          if (entry.getREFIT_INPUT_DIALOG()) {
            if ((entry).Item->ItemType == BoolValue) {
              ResultString += (entry).Item->BValue ? L": [+]" : L": [ ]";
            } else {
              ResultString += (entry).Item->SValue;
            }
          } else if (entry.getREFIT_MENU_CHECKBIT()) {
            // check boxes
            ResultString += ((entry).Item->IValue & (entry.Row)) ? L": [+]" : L": [ ]";
          } else if (entry.getREFIT_MENU_SWITCH()) {
            // radio buttons

            // update chosen config
            if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 90) {
              OldChosenItem = OldChosenConfig;
            } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 65) {
              OldChosenItem = OldChosenSmbios;
            } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 116) {
              OldChosenItem = OldChosenDsdt;
            } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 119) {
              OldChosenItem = OldChosenAudio;
            }

            ResultString += (entry.Row == OldChosenItem) ? L": (*)" : L": ( )";
          }
        }

        for ( j = ResultString.length() ; j < (INTN)TextMenuWidth; j++ ) {
          ResultString += L' ';
        }

        gST->ConOut->OutputString(gST->ConOut, ResultString.wc_str());
      }

      // scrolling indicators
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_SCROLLARROW);
      gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY);

      if (ScrollState.FirstVisible > 0) {
          gST->ConOut->OutputString (gST->ConOut, ArrowUp);
      } else {
          gST->ConOut->OutputString (gST->ConOut, L" ");
      }

      gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY + ScrollState.MaxVisible);

      if (ScrollState.LastVisible < ScrollState.MaxIndex) {
          gST->ConOut->OutputString (gST->ConOut, ArrowDown);
      } else {
          gST->ConOut->OutputString (gST->ConOut, L" ");
      }

      break;

    case MENU_FUNCTION_PAINT_SELECTION:
			// last selection
      // redraw selection cursor
      gST->ConOut->SetCursorPosition (gST->ConOut, 2, MenuPosY + (ScrollState.LastSelection - ScrollState.FirstVisible));
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_BASIC);
      //gST->ConOut->OutputString (gST->ConOut, DisplayStrings[ScrollState.LastSelection]);
			ResultString = Entries[ScrollState.LastSelection].Title;
      if (Entries[ScrollState.LastSelection].getREFIT_INPUT_DIALOG()) {
        REFIT_INPUT_DIALOG& entry = (REFIT_INPUT_DIALOG&) Entries[ScrollState.LastSelection];
        if (entry.getREFIT_INPUT_DIALOG()) {
          if (entry.Item->ItemType == BoolValue) {
            ResultString += entry.Item->BValue ? L": [+]" : L": [ ]";
          } else {
            ResultString += entry.Item->SValue;
          }
        } else if (entry.getREFIT_MENU_CHECKBIT()) {
          // check boxes
          ResultString += (entry.Item->IValue & (entry.Row)) ? L": [+]" : L": [ ]";
        } else if (entry.getREFIT_MENU_SWITCH()) {
          // radio buttons

          if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 90) {
            OldChosenItem = OldChosenConfig;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 65) {
            OldChosenItem = OldChosenSmbios;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 116) {
            OldChosenItem = OldChosenDsdt;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 119) {
            OldChosenItem = OldChosenAudio;
          }

          ResultString += (entry.Row == OldChosenItem) ? L": (*)" : L": ( )";
        }
      }

      for (j = ResultString.length(); j < (INTN) TextMenuWidth; j++) {
        ResultString += L' ';
      }

      gST->ConOut->OutputString (gST->ConOut, ResultString.wc_str());

        // current selection
      gST->ConOut->SetCursorPosition (gST->ConOut, 2, MenuPosY + (ScrollState.CurrentSelection - ScrollState.FirstVisible));
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_CURRENT);
      ResultString = Entries[ScrollState.CurrentSelection].Title;
      if (Entries[ScrollState.CurrentSelection].getREFIT_INPUT_DIALOG()) {
        REFIT_INPUT_DIALOG& entry = (REFIT_INPUT_DIALOG&) Entries[ScrollState.CurrentSelection];
        if (entry.getREFIT_INPUT_DIALOG()) {
          if (entry.Item->ItemType == BoolValue) {
            ResultString += entry.Item->BValue ? L": [+]" : L": [ ]";
          } else {
            ResultString += entry.Item->SValue;
          }
        } else if (entry.getREFIT_MENU_CHECKBIT()) {
          // check boxes
          ResultString += (entry.Item->IValue & (entry.Row)) ? L": [+]" : L": [ ]";
        } else if (entry.getREFIT_MENU_SWITCH()) {
          // radio buttons

          if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 90) {
            OldChosenItem = OldChosenConfig;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 65) {
            OldChosenItem = OldChosenSmbios;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 116) {
            OldChosenItem = OldChosenDsdt;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 119) {
            OldChosenItem = OldChosenAudio;
          }

          ResultString += (entry.Row == OldChosenItem) ? L": (*)" : L": ( )";
        }
      }

      for (j = ResultString.length() ; j < (INTN) TextMenuWidth; j++) {
        ResultString += L' ';
      }

      gST->ConOut->OutputString (gST->ConOut, ResultString.wc_str());
        //gST->ConOut->OutputString (gST->ConOut, DisplayStrings[ScrollState.CurrentSelection]);

      break;

    case MENU_FUNCTION_PAINT_TIMEOUT:
      if (ParamText[0] == 0) {
        // clear message
        gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
        gST->ConOut->SetCursorPosition (gST->ConOut, 0, ConHeight - 1);
        gST->ConOut->OutputString (gST->ConOut, BlankLine + 1);
      } else {
        // paint or update message
        gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
        gST->ConOut->SetCursorPosition (gST->ConOut, 3, ConHeight - 1);
        XStringW TimeoutMessage = SWPrintf("%ls  ", ParamText);
        gST->ConOut->OutputString (gST->ConOut, TimeoutMessage.wc_str());
      }

      break;
  }
}

/**
 * Draw text with specific coordinates.
 */


INTN REFIT_MENU_SCREEN::DrawTextXY(IN const XStringW& Text, IN INTN XPos, IN INTN YPos, IN UINT8 XAlign)
{
  INTN      TextWidth = 0;
  INTN      XText = 0;
  INTN      Height;
  INTN      TextXYStyle = 1;
  XImage TextBufferXY(0,0);

  if (Text.isEmpty()) {
    return 0;
  }
  //TODO assume using embedded font for BootScreen
  //messages must be TextXYStyle = 1 if it is provided by theme
  if (!ThemeX->getTextFace(1).valid) {
    if (ThemeX->getTextFace(2).valid) {
      TextXYStyle = 2;
    } else {
      TextXYStyle = 0;
    }
  }
/*
 * here we want to know how many place is needed for the graphical text
 * the procedure worked for fixed width font but the problem appears with proportional fonts
 * as well we not know yet the font using but egRenderText calculate later real width
 * so make a place to be large enoungh
 */
  ThemeX->MeasureText(Text, &TextWidth, NULL); //NULL means we already know Height
//  DBG("drawXY=%ls width=%lld\n", Text.wc_str(), TextWidth);
  if (XAlign == X_IS_LEFT) {
    TextWidth = UGAWidth - XPos - 1;
    XText = XPos;
  }

  if (!isBootScreen && ThemeX->TypeSVG) {
    TextWidth += ThemeX->TextHeight * 2; //give more place for buffer
    if (!ThemeX->getTextFace(TextXYStyle).valid) {
      DBG("no vaid text face for message!\n");
      Height = ThemeX->TextHeight;
    } else {
      Height = (int)(ThemeX->getTextFace(TextXYStyle).size * RowHeightFromTextHeight * ThemeX->Scale);
    }
  } else {
    Height = ThemeX->TextHeight;
  }
  OldTextHeight = Height;

//  TextBufferXY = egCreateFilledImage(TextWidth, Height, true, &MenuBackgroundPixel);
  TextBufferXY.setSizeInPixels(TextWidth, Height);
//  TextBufferXY.Fill(MenuBackgroundPixel);

  // render the text
  INTN TextWidth2 = ThemeX->RenderText(Text, &TextBufferXY, 0, 0, 0xFFFF, TextXYStyle);
  // there is real text width but we already have an array with Width = TextWidth
  //
//  TextBufferXY.EnsureImageSize(TextWidth2, Height); //assume color = MenuBackgroundPixel

  if (XAlign != X_IS_LEFT) {
    // shift 64 is prohibited
    XText = XPos - (TextWidth2 >> XAlign);  //X_IS_CENTER = 1
  }

  OldTextBufferRect.XPos = XText;
  OldTextBufferRect.YPos = YPos;
  OldTextBufferRect.Width = TextWidth2;
  OldTextBufferRect.Height = Height;

  OldTextBufferImage.GetArea(OldTextBufferRect);
  //GetArea may change sizes
  OldTextBufferRect.Width = OldTextBufferImage.GetWidth();
  OldTextBufferRect.Height = OldTextBufferImage.GetHeight();

  //  DBG("draw text %ls\n", Text);
  //  DBG("pos=%d width=%d xtext=%d Height=%d Y=%d\n", XPos, TextWidth, XText, Height, YPos);
 // TextBufferXY.Draw(XText, YPos, 0, false);
//  TextBufferXY.DrawWithoutCompose(XText, YPos);
  TextBufferXY.DrawOnBack(XText, YPos, ThemeX->Background);
  return TextWidth2;
}

void REFIT_MENU_SCREEN::EraseTextXY() //used on boot screen
{
  OldTextBufferImage.Draw(OldTextBufferRect.XPos, OldTextBufferRect.YPos);
}
/**
 * Helper function to draw text for Boot Camp Style.
 * @author: Needy
 */
void REFIT_MENU_SCREEN::DrawBCSText(IN CONST CHAR16 *Text, IN INTN XPos, IN INTN YPos, IN UINT8 XAlign)
{
  // check if text was provided. And what else?
  if (!Text) {
    return;
  }

  // number of chars to be drawn on the screen
  UINTN MaxTextLen = 13;

  // some optimization
  if (ThemeX->TileXSpace >= 25) {
    MaxTextLen = ThemeX->TileXSpace / 5 + 9;
  }

  XStringW BCSTextX;
  if (StrLen(Text) <= MaxTextLen) { // if the text exceeds the given limit
    BCSTextX.strncpy(Text, MaxTextLen);
  } else {
    BCSTextX.strncpy(Text, MaxTextLen - 2); // EllipsisLen=2
    BCSTextX += L"..";
  }
  DrawTextXY(BCSTextX, XPos, YPos, XAlign);
}

void REFIT_MENU_SCREEN::DrawMenuText(IN const XStringW& Text, IN INTN SelectedWidth, IN INTN XPos, IN INTN YPos, IN UINTN Cursor, IN INTN MaxWidth)
{
  INTN Width = (MaxWidth > 0 && (XPos + MaxWidth <= UGAWidth)) ? MaxWidth : UGAWidth - XPos;
  XImage TextBufferX(Width, ThemeX->TextHeight);
  XImage SelectionBar(Width, ThemeX->TextHeight);

/*
  if (Cursor == 0xFFFF) { //InfoLine = 0xFFFF
    TextBufferX.Fill(MenuBackgroundPixel);
  } else {
    TextBufferX.Fill(InputBackgroundPixel);
  }
*/

  if (SelectedWidth > 0) {
    // fill selection bar background
    EG_RECT TextRect;
    TextRect.Width = SelectedWidth;
    TextRect.Height = ThemeX->TextHeight;
    TextBufferX.FillArea(SelectionBackgroundPixel, TextRect);
//    SelectionBar.Fill(SelectionBackgroundPixel);
  }
  SelectionBar.CopyRect(ThemeX->Background, XPos, YPos);
//  SelectionBar.DrawWithoutCompose(XPos, YPos);
//  TextBufferX.Compose(0, 0, ThemeX->Background, true);
  // render the text
  if (ThemeX->TypeSVG) {
    //clovy - text vertically centred on Height
    ThemeX->RenderText(Text, &TextBufferX, 0,
                 (INTN)((ThemeX->TextHeight - (ThemeX->getTextFace(TextStyle).size * ThemeX->Scale)) / 2),
                 Cursor, TextStyle);
  } else {
    ThemeX->RenderText(Text, &TextBufferX, TEXT_XMARGIN, TEXT_YMARGIN, Cursor, TextStyle);
  }
  SelectionBar.Compose(0, 0, TextBufferX, false);
//  TextBufferX.DrawWithoutCompose(XPos, YPos);
  SelectionBar.DrawWithoutCompose(XPos, YPos);
}

void REFIT_MENU_SCREEN::SetBar(INTN PosX, INTN UpPosY, INTN DownPosY, IN SCROLL_STATE *State)
{
//  DBG("SetBar <= %d %d %d %d %d\n", UpPosY, DownPosY, State->MaxVisible, State->MaxIndex, State->FirstVisible);
//SetBar <= 302 722 19 31 0
  UpButton.XPos = PosX;
  UpButton.YPos = UpPosY;

  DownButton.XPos = UpButton.XPos;
  DownButton.YPos = DownPosY;

  ScrollbarBackground.XPos = UpButton.XPos;
  ScrollbarBackground.YPos = UpButton.YPos + UpButton.Height;
  ScrollbarBackground.Width = UpButton.Width;
  ScrollbarBackground.Height = DownButton.YPos - (UpButton.YPos + UpButton.Height);

  BarStart.XPos = ScrollbarBackground.XPos;
  BarStart.YPos = ScrollbarBackground.YPos;
  BarStart.Width = ScrollbarBackground.Width;

  BarEnd.Width = ScrollbarBackground.Width;
  BarEnd.XPos = ScrollbarBackground.XPos;
  BarEnd.YPos = DownButton.YPos - BarEnd.Height;

  ScrollStart.XPos = ScrollbarBackground.XPos;
  ScrollStart.YPos = ScrollbarBackground.YPos + ScrollbarBackground.Height * State->FirstVisible / (State->MaxIndex + 1);
  ScrollStart.Width = ScrollbarBackground.Width;

  Scrollbar.XPos = ScrollbarBackground.XPos;
  Scrollbar.YPos = ScrollStart.YPos + ScrollStart.Height;
  Scrollbar.Width = ScrollbarBackground.Width;
  Scrollbar.Height = ScrollbarBackground.Height * (State->MaxVisible + 1) / (State->MaxIndex + 1) - ScrollStart.Height;

  ScrollEnd.Width = ScrollbarBackground.Width;
  ScrollEnd.XPos = ScrollbarBackground.XPos;
  ScrollEnd.YPos = Scrollbar.YPos + Scrollbar.Height - ScrollEnd.Height;

  Scrollbar.Height -= ScrollEnd.Height;

  ScrollTotal.XPos = UpButton.XPos;
  ScrollTotal.YPos = UpButton.YPos;
  ScrollTotal.Width = UpButton.Width;
  ScrollTotal.Height = DownButton.YPos + DownButton.Height - UpButton.YPos;
//  DBG("ScrollTotal.Height = %d\n", ScrollTotal.Height);  //ScrollTotal.Height = 420
}

void REFIT_MENU_SCREEN::ScrollingBar()
{
  ScrollEnabled = (ScrollState.MaxFirstVisible != 0);
  if (!ScrollEnabled) {
    return;
  }
  //use compose instead of Draw
  //this is a copy of old algorithm
  // but we can not use Total and Draw all parts separately assumed they composed on background
  // it is #else

  XImage Total(ScrollTotal.Width, ScrollTotal.Height);
//  Total.Fill(&MenuBackgroundPixel);
  Total.CopyRect(ThemeX->Background, ScrollTotal.XPos, ScrollTotal.YPos);
  if (!ThemeX->ScrollbarBackgroundImage.isEmpty()) {
    for (INTN i = 0; i < ScrollbarBackground.Height; i+=ThemeX->ScrollbarBackgroundImage.GetHeight()) {
      Total.Compose(ScrollbarBackground.XPos - ScrollTotal.XPos, ScrollbarBackground.YPos + i - ScrollTotal.YPos, ThemeX->ScrollbarBackgroundImage, false);
    }
  }
  Total.Compose(BarStart.XPos - ScrollTotal.XPos, BarStart.YPos - ScrollTotal.YPos, ThemeX->BarStartImage, false);
  Total.Compose(BarEnd.XPos - ScrollTotal.XPos, BarEnd.YPos - ScrollTotal.YPos, ThemeX->BarEndImage, false);
  if (!ThemeX->ScrollbarImage.isEmpty()) {
    for (INTN i = 0; i < Scrollbar.Height; i+=ThemeX->ScrollbarImage.GetHeight()) {
      Total.Compose(Scrollbar.XPos - ScrollTotal.XPos, Scrollbar.YPos + i - ScrollTotal.YPos, ThemeX->ScrollbarImage, false);
    }
  }
  Total.Compose(UpButton.XPos - ScrollTotal.XPos, UpButton.YPos - ScrollTotal.YPos, ThemeX->UpButtonImage, false);
  Total.Compose(DownButton.XPos - ScrollTotal.XPos, DownButton.YPos - ScrollTotal.YPos, ThemeX->DownButtonImage, false);
  Total.Compose(ScrollStart.XPos - ScrollTotal.XPos, ScrollStart.YPos - ScrollTotal.YPos, ThemeX->ScrollStartImage, false);
  Total.Compose(ScrollEnd.XPos - ScrollTotal.XPos, ScrollEnd.YPos - ScrollTotal.YPos, ThemeX->ScrollEndImage, false);
  Total.Draw(ScrollTotal.XPos, ScrollTotal.YPos, ThemeX->ScrollWidth / 16.f); //ScrollWidth can be set in theme.plist but usually=16
}
/**
 * Graphical menu.
 */
void REFIT_MENU_SCREEN::GraphicsMenuStyle(IN UINTN Function, IN CONST CHAR16 *ParamText)
{
  INTN Chosen = 0;
  INTN ItemWidth = 0;
  INTN t1, t2;
  INTN VisibleHeight = 0; //assume vertical layout
  XStringW ResultString;
  INTN PlaceCentre = 0; //(TextHeight / 2) - 7;
  INTN PlaceCentre1 = 0;
  UINTN OldChosenItem = ~(UINTN)0;
  INTN TitleLen = 0;
  INTN ScaledWidth = (INTN)(ThemeX->CharWidth * ThemeX->Scale);

  // clovy
  INTN ctrlX, ctrlY, ctrlTextX;

  HidePointer();

  switch (Function) {

    case MENU_FUNCTION_INIT:
    {
      egGetScreenSize(&UGAWidth, &UGAHeight);
      InitAnime();
      SwitchToGraphicsAndClear();

      EntriesPosY = ((UGAHeight - (int)(LAYOUT_TOTAL_HEIGHT * ThemeX->Scale)) >> 1) + (int)(ThemeX->LayoutBannerOffset * ThemeX->Scale) + (ThemeX->TextHeight << 1);

      VisibleHeight = ((UGAHeight - EntriesPosY) / ThemeX->TextHeight) - InfoLines.size() - 2;
      //DBG("MENU_FUNCTION_INIT 1 EntriesPosY=%d VisibleHeight=%d\n", EntriesPosY, VisibleHeight);
      if ( Entries[0].getREFIT_INPUT_DIALOG() ) {
        REFIT_INPUT_DIALOG& entry = (REFIT_INPUT_DIALOG&)Entries[0];
        if (entry.getREFIT_MENU_SWITCH()) {
          if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 3) {
            Chosen = (OldChosenTheme == 0xFFFF) ? 0: (OldChosenTheme + 1);
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 65) {
            Chosen = OldChosenSmbios;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 90) {
            Chosen = OldChosenConfig;
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 116) {
            Chosen = (OldChosenDsdt == 0xFFFF) ? 0: (OldChosenDsdt + 1);
          } else if (entry.getREFIT_MENU_SWITCH()->Item->IValue == 119) {
            Chosen = OldChosenAudio;
          }
        }
      }
      InitScroll(Entries.size(), Entries.size(), VisibleHeight, Chosen);
      // determine width of the menu - not working
      //MenuWidth = 80;  // minimum
      MenuWidth = (int)(LAYOUT_TEXT_WIDTH * ThemeX->Scale); //500

      if (!TitleImage.isEmpty()) {
        if (MenuWidth > (INTN)(UGAWidth - (int)(TITLEICON_SPACING * ThemeX->Scale) - TitleImage.Image.GetWidth())) {
          MenuWidth = UGAWidth - (int)(TITLEICON_SPACING * ThemeX->Scale) - TitleImage.Image.GetWidth() - 2;
        }
        EntriesPosX = (UGAWidth - (TitleImage.Image.GetWidth() + (int)(TITLEICON_SPACING * ThemeX->Scale) + MenuWidth)) >> 1;
        //       DBG("UGAWIdth=%lld TitleImage.Image=%lld MenuWidth=%lld\n", UGAWidth,
        //           TitleImage.Image.GetWidth(), MenuWidth);
        MenuWidth += TitleImage.Image.GetWidth();
      } else {
        EntriesPosX = (UGAWidth - MenuWidth) >> 1;
      }
      TimeoutPosY = EntriesPosY + (Entries.size() + 1) * ThemeX->TextHeight;

      // set maximum allowed text length for menu content (this is applicable only for non-svg and non-proportional)
      MenuMaxTextLen = (UINTN)(MenuWidth / ScaledWidth);

      // initial painting
      ThemeX->MeasureText(Title, &ItemWidth, NULL);
      if (!(ThemeX->HideUIFlags & HIDEUI_FLAG_MENU_TITLE)) {
        DrawTextXY(Title, (UGAWidth >> 1), EntriesPosY - ThemeX->TextHeight * 2, X_IS_CENTER);
      }

      if (!TitleImage.isEmpty()) {
        INTN FilmXPos = (INTN)(EntriesPosX - (TitleImage.Image.GetWidth() + (int)(TITLEICON_SPACING * ThemeX->Scale)));
        INTN FilmYPos = (INTN)EntriesPosY;
        const XImage& tImage = TitleImage.GetBest(!Daylight);
   //     TitleImage.Image.Draw(FilmXPos, FilmYPos); //TODO - account night and svg

        // update FilmPlace only if not set by InitAnime
        if (FilmC->FilmPlace.Width == 0 || FilmC->FilmPlace.Height == 0) {
          FilmC->FilmPlace.XPos = FilmXPos;
          FilmC->FilmPlace.YPos = FilmYPos;
          FilmC->FilmPlace.Width = tImage.GetWidth();
          FilmC->FilmPlace.Height = tImage.GetHeight();
        }
        
        tImage.Draw(FilmXPos, FilmYPos);
      }

      if (InfoLines.size() > 0) {
        for (UINTN i = 0; i < InfoLines.size(); i++) {
          DrawMenuText(InfoLines[i], 0, EntriesPosX, EntriesPosY, 0xFFFF, 0);
          EntriesPosY += ThemeX->TextHeight;
        }
        EntriesPosY += ThemeX->TextHeight;  // also add a blank line
      }
      ThemeX->InitBar();

      break;
    }
    case MENU_FUNCTION_CLEANUP:
      HidePointer();
      break;

    case MENU_FUNCTION_PAINT_ALL:
    {
      //         DBG("PAINT_ALL: EntriesPosY=%lld MaxVisible=%lld\n", EntriesPosY, ScrollState.MaxVisible);
      //          DBG("DownButton.Height=%lld TextHeight=%lld MenuWidth=%lld\n", DownButton.Height, TextHeight, MenuWidth);
      t2 = EntriesPosY + (ScrollState.MaxVisible + 1) * ThemeX->TextHeight - DownButton.Height;
      t1 = EntriesPosX + ThemeX->TextHeight + MenuWidth  + (INTN)((TEXT_XMARGIN + 16) * ThemeX->Scale);
      //          DBG("PAINT_ALL: X=%lld Y=%lld\n", t1, t2);
      SetBar(t1, EntriesPosY, t2, &ScrollState); //823 302 554
      /*
       48:307  39:206  UGAWIdth=800 TitleImage=48 MenuWidth=333
       48:635  0:328  PAINT_ALL: EntriesPosY=259 MaxVisible=13
       48:640  0:004  DownButton.Height=0 TextHeight=21 MenuWidth=381
       48:646  0:006  PAINT_ALL: X=622 Y=553
       */

      // blackosx swapped this around so drawing of selection comes before drawing scrollbar.

      for (INTN i = ScrollState.FirstVisible, j = 0; i <= ScrollState.LastVisible; i++, j++) {
        REFIT_ABSTRACT_MENU_ENTRY *Entry = &Entries[i];
        ResultString = Entry->Title; //create a copy to modify later
        if (!ThemeX->TypeSVG && !ThemeX->Proportional && ResultString.length() > MenuMaxTextLen) {
          ResultString = ResultString.subString(0,MenuMaxTextLen-3) + L".."_XSW;
        }
        TitleLen = ResultString.length();
        Entry->Place.XPos = EntriesPosX;
        Entry->Place.YPos = EntriesPosY + j * ThemeX->TextHeight;
        Entry->Place.Width = TitleLen * ScaledWidth;
        Entry->Place.Height = (UINTN)ThemeX->TextHeight;
        PlaceCentre = (INTN)((ThemeX->TextHeight - (INTN)(ThemeX->Buttons[2].GetHeight())) * ThemeX->Scale / 2);
        PlaceCentre1 = (INTN)((ThemeX->TextHeight - (INTN)(ThemeX->Buttons[0].GetHeight())) * ThemeX->Scale / 2);
        // clovy
        ctrlX = (ThemeX->TypeSVG) ? EntriesPosX : EntriesPosX + (INTN)(TEXT_XMARGIN * ThemeX->Scale);
        ctrlTextX = ctrlX + ThemeX->Buttons[0].GetWidth() + (INTN)(TEXT_XMARGIN * ThemeX->Scale / 2);
        ctrlY = Entry->Place.YPos + PlaceCentre;

        if ( Entry->getREFIT_INPUT_DIALOG() ) {
          REFIT_INPUT_DIALOG* inputDialogEntry = Entry->getREFIT_INPUT_DIALOG();
          if (inputDialogEntry->Item && inputDialogEntry->Item->ItemType == BoolValue) {
            //possible artefacts
            DrawMenuText(ResultString, (i == ScrollState.CurrentSelection) ? (MenuWidth) : 0,
                         ctrlTextX, Entry->Place.YPos, 0xFFFF, MenuWidth);
            ThemeX->FillRectAreaOfScreen((ctrlTextX + ctrlX) >> 1, Entry->Place.YPos, ctrlTextX - ctrlX, ThemeX->TextHeight); //clean head
            ThemeX->Buttons[(inputDialogEntry->Item->BValue)?3:2].DrawOnBack(ctrlX, ctrlY, ThemeX->Background);
          } else {
            // text input
            ResultString += inputDialogEntry->Item->SValue + L" "_XSW;
            // set cursor to beginning if it is outside of screen
            if (!ThemeX->TypeSVG && !ThemeX->Proportional && (TitleLen + (INTN)Entry->Row) * ScaledWidth > MenuWidth) {
              Entry->Row = 0;
            }
            // Slice - suppose to use Row as Cursor in text
            DrawMenuText(ResultString, (i == ScrollState.CurrentSelection) ? MenuWidth : 0,
                         EntriesPosX, Entry->Place.YPos, TitleLen + Entry->Row, MenuWidth);
            ThemeX->FillRectAreaOfScreen(MenuWidth + ((ctrlTextX + EntriesPosX) >> 1), Entry->Place.YPos, ctrlTextX - EntriesPosX, ThemeX->TextHeight); //clean tail
          }
        } else if (Entry->getREFIT_MENU_CHECKBIT()) {
          DrawMenuText(ResultString, (i == ScrollState.CurrentSelection) ? (MenuWidth) : 0,
                       ctrlTextX, Entry->Place.YPos, 0xFFFF, MenuWidth);
          ThemeX->FillRectAreaOfScreen((ctrlTextX + ctrlX) >> 1, Entry->Place.YPos, ctrlTextX - ctrlX, ThemeX->TextHeight); //clean head
          ThemeX->Buttons[(((REFIT_INPUT_DIALOG*)(Entry))->Item->IValue & Entry->Row)?3:2].DrawOnBack(ctrlX, ctrlY, ThemeX->Background);
        } else if (Entry->getREFIT_MENU_SWITCH()) {
          if (Entry->getREFIT_MENU_SWITCH()->Item->IValue == 3) {
            //OldChosenItem = OldChosenTheme;
            OldChosenItem = (OldChosenTheme == 0xFFFF) ? 0: (OldChosenTheme + 1);
          } else if (Entry->getREFIT_MENU_SWITCH()->Item->IValue == 65) {
            OldChosenItem = OldChosenSmbios;
          } else if (Entry->getREFIT_MENU_SWITCH()->Item->IValue == 90) {
            OldChosenItem = OldChosenConfig;
          } else if (Entry->getREFIT_MENU_SWITCH()->Item->IValue == 116) {
            OldChosenItem =  (OldChosenDsdt == 0xFFFF) ? 0: (OldChosenDsdt + 1);
          } else if (Entry->getREFIT_MENU_SWITCH()->Item->IValue == 119) {
            OldChosenItem = OldChosenAudio;
          }
          DrawMenuText(ResultString, (i == ScrollState.CurrentSelection) ? MenuWidth : 0,
                       // clovy                  EntriesPosX + (TextHeight + (INTN)(TEXT_XMARGIN * GlobalConfig.Scale)),
                       ctrlTextX, Entry->Place.YPos, 0xFFFF, MenuWidth);
          ThemeX->FillRectAreaOfScreen((ctrlTextX + ctrlX) >> 1, Entry->Place.YPos, ctrlTextX - ctrlX, ThemeX->TextHeight); //clean head
          ThemeX->Buttons[(Entry->Row == OldChosenItem)?1:0].DrawOnBack(ctrlX, ctrlY, ThemeX->Background);
        } else {
          //DBG("paint entry %d title=%ls\n", i, Entries[i]->Title);
          DrawMenuText(ResultString, (i == ScrollState.CurrentSelection) ? MenuWidth : 0,
                       EntriesPosX, Entry->Place.YPos, 0xFFFF, MenuWidth);
          ThemeX->FillRectAreaOfScreen(MenuWidth + ((ctrlTextX + EntriesPosX) >> 1), Entry->Place.YPos, ctrlTextX - EntriesPosX, ThemeX->TextHeight); //clean tail
        }
      }

      ScrollingBar(); //&ScrollState - inside the class
      //MouseBirth();
      break;
    }
    case MENU_FUNCTION_PAINT_SELECTION:
    {
      REFIT_ABSTRACT_MENU_ENTRY *EntryL = &Entries[ScrollState.LastSelection];
      REFIT_ABSTRACT_MENU_ENTRY *EntryC = &Entries[ScrollState.CurrentSelection];

      // last selection
      ResultString = EntryL->Title;
      if (!ThemeX->TypeSVG && !ThemeX->Proportional && ResultString.length() > MenuMaxTextLen) {
        ResultString = ResultString.subString(0,MenuMaxTextLen-3) + L".."_XSW;
      }
      TitleLen = ResultString.length();
      //clovy//PlaceCentre = (TextHeight - (INTN)(Buttons[2]->Height * GlobalConfig.Scale)) / 2;
      //clovy//PlaceCentre = (PlaceCentre>0)?PlaceCentre:0;
      //clovy//PlaceCentre1 = (TextHeight - (INTN)(Buttons[0]->Height * GlobalConfig.Scale)) / 2;
      PlaceCentre = (INTN)((ThemeX->TextHeight - (INTN)(ThemeX->Buttons[2].GetHeight())) * ThemeX->Scale / 2);
      PlaceCentre1 = (INTN)((ThemeX->TextHeight - (INTN)(ThemeX->Buttons[0].GetHeight())) * ThemeX->Scale / 2);

      // clovy
      ctrlX = (ThemeX->TypeSVG) ? EntriesPosX : EntriesPosX + (INTN)(TEXT_XMARGIN * ThemeX->Scale);
      ctrlTextX = ctrlX + ThemeX->Buttons[0].GetWidth() + (INTN)(TEXT_XMARGIN * ThemeX->Scale / 2);

      // redraw selection cursor
      // 1. blackosx swapped this around so drawing of selection comes before drawing scrollbar.
      // 2. usr-sse2
      if ( EntryL->getREFIT_INPUT_DIALOG() ) {
        REFIT_INPUT_DIALOG* inputDialogEntry = (REFIT_INPUT_DIALOG*)EntryL;
        if (inputDialogEntry->Item->ItemType == BoolValue) { //this is checkbox
          //clovy
          DrawMenuText(ResultString, 0,
                       ctrlTextX, EntryL->Place.YPos, 0xFFFF, MenuWidth);
          ThemeX->Buttons[(inputDialogEntry->Item->BValue)?3:2].DrawOnBack(ctrlX, EntryL->Place.YPos + PlaceCentre, ThemeX->Background);
        } else {
          ResultString += (inputDialogEntry->Item->SValue.wc_str() + inputDialogEntry->Item->LineShift) + L" "_XSW;
          DrawMenuText(ResultString, 0,
                       EntriesPosX, EntryL->Place.YPos, TitleLen + EntryL->Row, MenuWidth);
          ThemeX->FillRectAreaOfScreen(MenuWidth + ((ctrlTextX + EntriesPosX) >> 1), EntryL->Place.YPos, ctrlTextX - EntriesPosX, ThemeX->TextHeight); //clean tail
        }
      } else if (EntryL->getREFIT_MENU_SWITCH()) { //radio buttons 0,1
        if (EntryL->getREFIT_MENU_SWITCH()->Item->IValue == 3) {
          OldChosenItem = (OldChosenTheme == 0xFFFF) ? 0: OldChosenTheme + 1;
        } else if (EntryL->getREFIT_MENU_SWITCH()->Item->IValue == 65) {
          OldChosenItem = OldChosenSmbios;
        } else if (EntryL->getREFIT_MENU_SWITCH()->Item->IValue == 90) {
          OldChosenItem = OldChosenConfig;
        } else if (EntryL->getREFIT_MENU_SWITCH()->Item->IValue == 116) {
          OldChosenItem = (OldChosenDsdt == 0xFFFF) ? 0: OldChosenDsdt + 1;
        } else if (EntryL->getREFIT_MENU_SWITCH()->Item->IValue == 119) {
          OldChosenItem = OldChosenAudio;
        }
        // clovy
        DrawMenuText(ResultString, 0,
                     ctrlTextX, EntryL->Place.YPos, 0xFFFF, MenuWidth);
        ThemeX->Buttons[(EntryL->Row == OldChosenItem)?1:0].DrawOnBack(ctrlX, EntryL->Place.YPos + PlaceCentre1, ThemeX->Background);
      } else if (EntryL->getREFIT_MENU_CHECKBIT()) {
        // clovy
        DrawMenuText(ResultString, 0,
                     ctrlTextX, EntryL->Place.YPos, 0xFFFF, MenuWidth);
        ThemeX->Buttons[(EntryL->getREFIT_MENU_CHECKBIT()->Item->IValue & EntryL->Row) ?3:2].DrawOnBack(ctrlX, EntryL->Place.YPos + PlaceCentre, ThemeX->Background);
      } else {
        DrawMenuText(ResultString, 0,
                     EntriesPosX, EntryL->Place.YPos, 0xFFFF, MenuWidth);
        ThemeX->FillRectAreaOfScreen(MenuWidth + ((ctrlTextX + EntriesPosX) >> 1), EntryL->Place.YPos, ctrlTextX - EntriesPosX, ThemeX->TextHeight); //clean tail
      }

      // current selection
      ResultString = EntryC->Title;
      if (!ThemeX->TypeSVG && !ThemeX->Proportional && ResultString.length() > MenuMaxTextLen) {
        ResultString = ResultString.subString(0,MenuMaxTextLen-3) + L".."_XSW;
      }
      TitleLen = ResultString.length();
      if ( EntryC->getREFIT_MENU_SWITCH() ) {
        if (EntryC->getREFIT_MENU_SWITCH()->Item->IValue == 3) {
          OldChosenItem = (OldChosenTheme == 0xFFFF) ? 0: OldChosenTheme + 1;
        } else if (EntryC->getREFIT_MENU_SWITCH()->Item->IValue == 65) {
          OldChosenItem = OldChosenSmbios;
        } else if (EntryC->getREFIT_MENU_SWITCH()->Item->IValue == 90) {
          OldChosenItem = OldChosenConfig;
        } else if (EntryC->getREFIT_MENU_SWITCH()->Item->IValue == 116) {
          OldChosenItem = (OldChosenDsdt == 0xFFFF) ? 0: OldChosenDsdt + 1;
        } else if (EntryC->getREFIT_MENU_SWITCH()->Item->IValue == 119) {
          OldChosenItem = OldChosenAudio;
        }
      }

      if ( EntryC->getREFIT_INPUT_DIALOG() ) {
        REFIT_INPUT_DIALOG* inputDialogEntry = (REFIT_INPUT_DIALOG*)EntryC;
        if (inputDialogEntry->Item->ItemType == BoolValue) { //checkbox
          DrawMenuText(ResultString, MenuWidth,
                       ctrlTextX, EntryC->Place.YPos, 0xFFFF, MenuWidth);
          ThemeX->Buttons[(inputDialogEntry->Item->BValue)?3:2].DrawOnBack(ctrlX, EntryC->Place.YPos + PlaceCentre, ThemeX->Background);
        } else {
          ResultString += (inputDialogEntry->Item->SValue.wc_str() + inputDialogEntry->Item->LineShift) + L" "_XSW;
          DrawMenuText(ResultString, MenuWidth,
                       EntriesPosX, EntryC->Place.YPos, TitleLen + EntryC->Row, MenuWidth);
        }
      } else if (EntryC->getREFIT_MENU_SWITCH()) { //radio
        DrawMenuText(ResultString, MenuWidth,
                     ctrlTextX, EntryC->Place.YPos, 0xFFFF, MenuWidth);
        ThemeX->Buttons[(EntryC->Row == OldChosenItem)?1:0].DrawOnBack(ctrlX, EntryC->Place.YPos + PlaceCentre1, ThemeX->Background);
      } else if (EntryC->getREFIT_MENU_CHECKBIT()) {
        DrawMenuText(ResultString, MenuWidth,
                     ctrlTextX, EntryC->Place.YPos, 0xFFFF, MenuWidth);
        ThemeX->Buttons[(EntryC->getREFIT_MENU_CHECKBIT()->Item->IValue & EntryC->Row)?3:2].DrawOnBack(ctrlX, EntryC->Place.YPos + PlaceCentre, ThemeX->Background);
      } else {
        DrawMenuText(ResultString, MenuWidth,
                     EntriesPosX, EntryC->Place.YPos, 0xFFFF, MenuWidth);
      }

      ScrollStart.YPos = ScrollbarBackground.YPos + ScrollbarBackground.Height * ScrollState.FirstVisible / (ScrollState.MaxIndex + 1);
      Scrollbar.YPos = ScrollStart.YPos + ScrollStart.Height;
      ScrollEnd.YPos = Scrollbar.YPos + Scrollbar.Height; // ScrollEnd.Height is already subtracted
      ScrollingBar(); //&ScrollState);

      break;
    }

    case MENU_FUNCTION_PAINT_TIMEOUT: //ParamText should be XStringW
      ResultString.takeValueFrom(ParamText);
      INTN X = (UGAWidth - StrLen(ParamText) * ScaledWidth) >> 1;
      DrawMenuText(ResultString, 0, X, TimeoutPosY, 0xFFFF, 0);
      break;
  }

  MouseBirth();
}

//
// user-callable dispatcher functions
//

UINTN REFIT_MENU_SCREEN::RunMenu(OUT REFIT_ABSTRACT_MENU_ENTRY **ChosenEntry)
{
  INTN Index = -1;

  return RunGenericMenu(&Index, ChosenEntry);
}


void REFIT_MENU_SCREEN::AddMenuCheck(CONST CHAR8 *Text, UINTN Bit, INTN ItemNum)
{
  REFIT_MENU_CHECKBIT *InputBootArgs;

  InputBootArgs = new REFIT_MENU_CHECKBIT;
  InputBootArgs->Title.takeValueFrom(Text);
//  InputBootArgs->Tag = TAG_CHECKBIT_OLD;
  InputBootArgs->Row = Bit;
  InputBootArgs->Item = &InputItems[ItemNum];
  InputBootArgs->AtClick = ActionEnter;
  InputBootArgs->AtRightClick = ActionDetails;
  AddMenuEntry(InputBootArgs, true);
}


void REFIT_MENU_SCREEN::AddMenuItem_(REFIT_MENU_ENTRY_ITEM_ABSTRACT* InputBootArgs, INTN Inx, CONST CHAR8 *Line, XBool Cursor)
{
  InputBootArgs->Title.takeValueFrom(Line);
  if (Inx == 3 || Inx == 116) {
    InputBootArgs->Row          = 0;
  } else {
    InputBootArgs->Row          = Cursor?InputItems[Inx].SValue.length():0xFFFF;
  }
  InputBootArgs->Item           = &InputItems[Inx];
  InputBootArgs->AtClick        = Cursor?ActionSelect:ActionEnter;
  InputBootArgs->AtRightClick   = Cursor?ActionNone:ActionDetails;
  InputBootArgs->AtDoubleClick  = Cursor?ActionEnter:ActionNone;

  AddMenuEntry(InputBootArgs, true);
}

void REFIT_MENU_SCREEN::AddMenuItemInput(INTN Inx, CONST CHAR8 *Line, XBool Cursor)
{
  REFIT_INPUT_DIALOG *InputBootArgs = new REFIT_INPUT_DIALOG;
  AddMenuItem_(InputBootArgs, Inx, Line, Cursor);
}

void REFIT_MENU_SCREEN::AddMenuItemSwitch(INTN Inx, CONST CHAR8 *Line, XBool Cursor)
{
  REFIT_MENU_SWITCH *InputBootArgs = new REFIT_MENU_SWITCH;
  AddMenuItem_(InputBootArgs, Inx, Line, Cursor);
}


EFI_STATUS REFIT_MENU_SCREEN::CheckMouseEvent()
{
  EFI_STATUS Status = EFI_TIMEOUT;
  mAction = ActionNone;
  MOUSE_EVENT Event = mPointer.GetEvent();
  XBool Move = false;

  if (!IsDragging && Event == MouseMove)
    Event = NoEvents;

  if (ScrollEnabled){
    if (mPointer.MouseInRect(&UpButton) && Event == LeftClick)
      mAction = ActionScrollUp;
    else if (mPointer.MouseInRect(&DownButton) && Event == LeftClick)
      mAction = ActionScrollDown;
    else if (mPointer.MouseInRect(&Scrollbar) && Event == LeftMouseDown) {
      IsDragging = true;
      Move = true;
//      mAction = ActionMoveScrollbar;
      ScrollbarYMovement = 0;
      ScrollbarOldPointerPlace.XPos = ScrollbarNewPointerPlace.XPos = mPointer.GetPlace().XPos;
      ScrollbarOldPointerPlace.YPos = ScrollbarNewPointerPlace.YPos = mPointer.GetPlace().YPos;
    }
    else if (IsDragging && Event == LeftClick) {
      IsDragging = false;
      Move = true;
//      mAction = ActionMoveScrollbar;
    }
    else if (IsDragging && Event == MouseMove) {
      mAction = ActionMoveScrollbar;
      ScrollbarNewPointerPlace.XPos = mPointer.GetPlace().XPos;
      ScrollbarNewPointerPlace.YPos = mPointer.GetPlace().YPos;
    }
    else if (mPointer.MouseInRect(&ScrollbarBackground) &&
             Event == LeftClick) {
      if (mPointer.GetPlace().YPos < Scrollbar.YPos) // up
        mAction = ActionPageUp;
      else // down
        mAction = ActionPageDown;
    // page up/down, like in OS X
    }
    else if (Event == ScrollDown) {
      mAction = ActionScrollDown;
    }
    else if (Event == ScrollUp) {
      mAction = ActionScrollUp;
    }
  }
  if (!ScrollEnabled || (mAction == ActionNone && !Move) ) {
      for (UINTN EntryId = 0; EntryId < Entries.size(); EntryId++) {
        if (mPointer.MouseInRect(&(Entries[EntryId].Place))) {
          switch (Event) {
            case LeftClick:
              mAction = Entries[EntryId].AtClick;
              //          DBG("Click\n");
              break;
            case RightClick:
              mAction = Entries[EntryId].AtRightClick;
              break;
            case DoubleClick:
              mAction = Entries[EntryId].AtDoubleClick;
              break;
            case ScrollDown:
              mAction = ActionScrollDown;
              break;
            case ScrollUp:
              mAction = ActionScrollUp;
              break;
            case MouseMove:
              mAction = Entries[EntryId].AtMouseOver;
              //how to do the action once?
              break;
            default:
              mAction = ActionNone;
              break;
          }
          mItemID = EntryId;
          break;
        }
        else { //click in milk
          switch (Event) {
            case LeftClick:
              mAction = ActionDeselect;
              break;
            case RightClick:
              mAction = ActionFinish;
              break;
            case ScrollDown:
              mAction = ActionScrollDown;
              break;
            case ScrollUp:
              mAction = ActionScrollUp;
              break;
            default:
              mAction = ActionNone;
              break;
          }
          mItemID = 0xFFFF;
        }
      }

  }

  if (mAction != ActionNone) {
    Status = EFI_SUCCESS;
 //   Event = NoEvents; //clear event as set action
    mPointer.ClearEvent();
  }
  return Status;
}


//Screen.UpdateAnime(); called from Menu cycle wait for event

// object XCinema Cinema is a part of Theme
// object FILM* FilmC is a part or current Screen. Must be initialized from Cinema somewhere on Screen init
// assumed one Film per screen
void REFIT_MENU_SCREEN::UpdateFilm()
{
  if (FilmC == nullptr || !FilmC->AnimeRun) {
//    DBG("no anime -> run=%d\n", FilmC->AnimeRun?1:0);
    return;
  }
  // here we propose each screen has own link to a Film
  INT64      Now = AsmReadTsc();

  if (FilmC->LastDraw == 0) {
    DBG("=== Update Film ===\n");
    DBG("FilmX=%lld\n", FilmC->FilmX);
    DBG("ID=%lld\n", FilmC->GetIndex());
    DBG("RunOnce=%d\n", FilmC->RunOnce?1:0);
    DBG("NumFrames=%lld\n", FilmC->NumFrames);
    DBG("FrameTime=%lld\n", FilmC->FrameTime);
    DBG("Path=%ls\n", FilmC->Path.wc_str());
    DBG("LastFrame=%lld\n\n", FilmC->LastFrameID());
  }

  if (TimeDiff(FilmC->LastDraw, Now) < (UINTN)FilmC->FrameTime) return;

  XImage Frame = FilmC->GetImage(); //take current image
  if (!Frame.isEmpty()) {
    Frame.DrawOnBack(FilmC->FilmPlace.XPos, FilmC->FilmPlace.YPos, ThemeX->Background);
  }
  FilmC->Advance(); //next frame no matter if previous was not found
  if (FilmC->Finished()) { //first loop finished
    FilmC->AnimeRun = !FilmC->RunOnce; //will stop anime if it set as RunOnce
  }
  FilmC->LastDraw = Now;
}
