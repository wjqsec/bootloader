//
//  StartupSound.c
//  Clover
//
//  Created by Slice on 02.01.2019.
//
// based on AudioPkg by Goldfish64
// https://github.com/Goldfish64/AudioPkg
/*
* Copyright (c) 2018 John Davis
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


#include <Platform.h> // Only use angled for Platform, else, xcode project won't compile
#include <Efi.h>
#include "StartupSound.h"
#include "Settings.h"
#include "Nvram.h"

extern UINT8 EmbeddedSound[];
extern UINTN EmbeddedSoundLength;


#ifndef DEBUG_ALL
#define DEBUG_SOUND 1
#else
#define DEBUG_SOUND DEBUG_ALL
#endif

#if DEBUG_SOUND == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_SOUND, __VA_ARGS__)
#endif



EFI_AUDIO_IO_PROTOCOL *AudioIo = NULL;


EFI_STATUS
StartupSoundPlay(const EFI_FILE* Dir, CONST CHAR16* SoundFile)
{
  EFI_STATUS Status;
  UINT8           *FileData = NULL;
  UINTN           FileDataLength = 0U;
  WAVE_FILE_DATA  WaveData;
  UINT8           OutputVolume = DefaultAudioVolume;
  UINT16          *TempData = NULL;
  UINTN           Len;
  
  if (OldChosenAudio >= AudioList.size()) {
    OldChosenAudio = 0; //security correction
  }
  size_t OutputIndex = (OldChosenAudio & 0xFF);

  WaveData.Samples = NULL;
  WaveData.SamplesLength = 0;
  if (!AudioIo) {
    Status = EFI_DEVICE_ERROR;
    //    DBG("not found AudioIo to play\n");
    goto DONE_ERROR;
  }

  if (SoundFile) {
    Status = egLoadFile(Dir, SoundFile, &FileData, &FileDataLength);
    if (EFI_ERROR(Status)) {
//      DBG("file sound read: %ls %s\n", SoundFile, efiStrError(Status));
      goto DONE_ERROR;
    }
  } else {
    FileData = EmbeddedSound;
    FileDataLength = EmbeddedSoundLength;
//    DBG("got embedded sound\n");
  }

  
  Status = WaveGetFileData(FileData, (UINT32)FileDataLength, &WaveData); //
  if (EFI_ERROR(Status)) {
    MsgLog(" wrong sound file, wave status=%s\n", efiStrError(Status));
    //if error then data not allocated
    goto DONE_ERROR;
  }
  Len = WaveData.SamplesLength; //byte length
	MsgLog("  Channels: %hu  Sample rate: %u Hz  Bits: %hu\n", WaveData.Format->Channels, WaveData.Format->SamplesPerSec, WaveData.Format->BitsPerSample);

  EFI_AUDIO_IO_PROTOCOL_BITS bits;
  switch (WaveData.Format->BitsPerSample) {
    case 8:
      bits = EfiAudioIoBits8;
      break;
    case 16:
      bits = EfiAudioIoBits16;
      break;
    case 20:
      bits = EfiAudioIoBits20;
      break;
    case 24:
      bits = EfiAudioIoBits24;
      break;
    case 32:
      bits = EfiAudioIoBits32;
      break;
    default:
      goto DONE_ERROR;
  }

  EFI_AUDIO_IO_PROTOCOL_FREQ freq;
  switch (WaveData.Format->SamplesPerSec) {
    case 8000:
      freq = EfiAudioIoFreq8kHz;
      break;
    case 11000:
      freq = EfiAudioIoFreq11kHz;
      break;
    case 16000:
      freq = EfiAudioIoFreq16kHz;
      break;
    case 22050:
      freq = EfiAudioIoFreq22kHz;
      break;
    case 32000:
      freq = EfiAudioIoFreq32kHz;
      break;
    case 44100:
      freq = EfiAudioIoFreq44kHz;
      break;
    case 48000:
      freq = EfiAudioIoFreq48kHz;
      break;
    case 88000:
      freq = EfiAudioIoFreq88kHz;
      break;
    case 96000:
      freq = EfiAudioIoFreq96kHz;
      break;
    case 192000:
      freq = EfiAudioIoFreq192kHz;
      break;
    default:
      goto DONE_ERROR;
  }

  DBG("output to channel %zu with volume %d, len=%d\n", OutputIndex, OutputVolume, WaveData.SamplesLength);
  DBG(" sound channels=%d bits=%d freq=%d\n", WaveData.Format->Channels, WaveData.Format->BitsPerSample, WaveData.Format->SamplesPerSec);

  if (!WaveData.SamplesLength || !OutputVolume) {
//    DBG("nothing to play\n"); //but data allocated
    Status = EFI_NOT_FOUND;
    goto DONE_ERROR;
  }

  if ((freq == EfiAudioIoFreq8kHz) && (bits == EfiAudioIoBits16)) {
    //making conversion
    Len *= 6; //8000<->48000
    UINTN Ind, Out=0, Tact;
    INT16 Tmp, Next;
    float Delta;
    INT16 *Ptr = (INT16*)WaveData.Samples;
    if (!Ptr) {
      Status = EFI_NOT_FOUND;
 //     DBG("not found wave data\n");
      goto DONE_ERROR;
    }
//    TempData = (__typeof__(TempData))AllocateZeroPool(Len * sizeof(INT16));
    TempData = (__typeof__(TempData))AllocateAlignedPages(EFI_SIZE_TO_PAGES(Len + 4095), 128);
    Tmp = *(Ptr++);
    for (Ind = 0; Ind < WaveData.SamplesLength / 2 - 1; Ind++) {
      Next = *(Ptr++);
      Delta = (Next - Tmp) / 6.f;
      for (Tact = 0; Tact < 6; Tact++) {
        TempData[Out++] = Tmp;
        Tmp = (INT16)(Delta + Tmp + 0.5f);
      }
      Tmp = Next;
    }
    freq = EfiAudioIoFreq48kHz;
    // Samples was allocated via AllocateAlignedPages, so it must not be freed with FreePool, but according to the number of pages allocated
    FreeAlignedPages(WaveData.Samples,EFI_SIZE_TO_PAGES(WaveData.SamplesLength + 4095));
    WaveData.SamplesLength *= 6;
    DBG("sound converted to 48kHz\n");
    WaveData.Samples = (UINT8*)TempData;
  }

  // Setup playback.
  if (OutputIndex >= AudioList.size()) {
    OutputIndex = 0;
    DBG("wrong index for Audio output\n");
  }
  Status = AudioIo->SetupPlayback(AudioIo, AudioList[OutputIndex].Index, OutputVolume,
                                  freq, bits, (UINT8)(WaveData.Format->Channels));
  if (EFI_ERROR(Status)) {
    MsgLog("StartupSound: Error setting up playback: %s\n", efiStrError(Status));
    goto DONE_ERROR;
  }
//  DBG("playback set\n");
  // Start playback.
  if (gSettings.GUI.PlayAsync) {
    Status = AudioIo->StartPlaybackAsync(AudioIo, WaveData.Samples, WaveData.SamplesLength, 0, NULL, NULL);
//    DBG("async started, status=%s\n", efiStrError(Status));
  } else {
    Status = AudioIo->StartPlayback(AudioIo, WaveData.Samples, WaveData.SamplesLength, 0);
//    DBG("sync started, status=%s\n", efiStrError(Status));
//    if (!EFI_ERROR(Status)) {
//      FreePool(TempData);
//    }
  }

  if (EFI_ERROR(Status)) {
    MsgLog("StartupSound: Error starting playback: %s\n", efiStrError(Status));
  }

DONE_ERROR:
  if (FileData && SoundFile) {  //dont free embedded sound
//    DBG("free sound\n");
    FreePool(FileData);
  }
  if (!gSettings.GUI.PlayAsync && WaveData.Samples) {
    //dont free sound when async play
    // here we have memory leak with WaveData.Samples
    // and we can't free memory up to stop AsyncPlay
    FreeAlignedPages(WaveData.Samples, EFI_SIZE_TO_PAGES(WaveData.SamplesLength + 4095));
  }
  DBG("sound play end with status=%s\n", efiStrError(Status));
  return Status;
}

EFI_STATUS
GetStoredOutput()
{
  // Create variables.
  EFI_STATUS Status;
  UINTN h;

  // Device Path.
  EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
  UINT8 *StoredDevicePath = NULL; //it is EFI_DEVICE_PATH_PROTOCOL*
  UINTN StoredDevicePathSize = 0;

  // Audio I/O.
  EFI_HANDLE *AudioIoHandles = NULL;
  UINTN AudioIoHandleCount = 0;
  EFI_AUDIO_IO_PROTOCOL *AudioIoProto = NULL;

  // Output.
  UINTN OutputPortIndex;
  UINTN OutputPortIndexSize = sizeof(OutputPortIndex);
  UINT8 OutputVolume;
  UINTN OutputVolumeSize = sizeof(OutputVolume);

  // Get Audio I/O protocols.
  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiAudioIoProtocolGuid, NULL, &AudioIoHandleCount, &AudioIoHandles);
  if (EFI_ERROR(Status)) {
    MsgLog("No AudioIoProtocol, status=%s\n", efiStrError(Status));
    goto DONE;
  }
	DBG("found %llu handles with audio\n", AudioIoHandleCount);
  // Get stored device path size. First from AppleBootGuid
  StoredDevicePath = (__typeof__(StoredDevicePath))GetNvramVariable(L"Clover.SoundDevice", gEfiAppleBootGuid, NULL, &StoredDevicePathSize);
  if (!StoredDevicePath) {
    // second attempt with BootChimeGuid
    StoredDevicePath = (__typeof__(StoredDevicePath))GetNvramVariable(BOOT_CHIME_VAR_DEVICE, gBootChimeVendorVariableGuid, NULL, &StoredDevicePathSize);
    if (!StoredDevicePath) {
      MsgLog("No AudioIoDevice stored\n");
      Status = EFI_NOT_FOUND;
      goto DONE;
    }
  }

  //we have to convert str->data if happen
  if ((StoredDevicePath[0] != 2) && (StoredDevicePath[1] != 1)) {
    XStringW StoredDevicePathStr = SWPrintf("%ls", (CHAR16*)StoredDevicePath);
    FreePool(StoredDevicePath);
    DBG("stored device=%ls\n", StoredDevicePathStr.wc_str());
    StoredDevicePath = (UINT8*)ConvertTextToDevicePath(StoredDevicePathStr.wc_str());
    StoredDevicePathSize = GetDevicePathSize((EFI_DEVICE_PATH_PROTOCOL *)StoredDevicePath);
  }

  // Try to find the matching device exposing an Audio I/O protocol.
  for (h = 0; h < AudioIoHandleCount; h++) {
    // Open Device Path protocol.
    Status = gBS->HandleProtocol(AudioIoHandles[h], &gEfiDevicePathProtocolGuid, (void**)&DevicePath);
    if (EFI_ERROR(Status)) {
		DBG("no DevicePath at %llu handle AudioIo\n", h);
      continue;
    }

    // Compare Device Paths. If they match, we have our Audio I/O device.
    if (!CompareMem(StoredDevicePath, DevicePath, StoredDevicePathSize)) {
      // Open Audio I/O protocol.
      Status = gBS->HandleProtocol(AudioIoHandles[h], &gEfiAudioIoProtocolGuid, (void**)&AudioIoProto);
      if (EFI_ERROR(Status)) {
        DBG("dont handle AudioIo\n");
        goto DONE;
      }
      break;
    }
  }

  // If the Audio I/O variable is still null, we couldn't find it.
  if (AudioIoProto == NULL) {
    Status = EFI_NOT_FOUND;
    DBG("not found AudioIo in nvram\n");
    goto DONE;
  }

  // Get stored device index.
  OutputPortIndex = 0;
  Status = gRT->GetVariable(L"Clover.SoundIndex", gEfiAppleBootGuid, NULL,
                            &OutputPortIndexSize, &OutputPortIndex);
  if (EFI_ERROR(Status)) {
    Status = gRT->GetVariable(BOOT_CHIME_VAR_INDEX, gBootChimeVendorVariableGuid, NULL,
                              &OutputPortIndexSize, &OutputPortIndex);
    if (EFI_ERROR(Status)) {
      MsgLog("Bad output index, status=%s, set 0\n", efiStrError(Status));
      OutputPortIndex = 0;
    }
  }
  OutputPortIndex &= 0x2F;
	DBG("got index=%llu\n", OutputPortIndex);
  if (OutputPortIndex >= AudioList.size()) {
	  DBG("... but max=%zu, so reset to 0\n", AudioList.size());
    OutputPortIndex = 0;
  }
  // Get stored volume. If this fails, just use the max.
  OutputVolume = DefaultAudioVolume;
  Status = gRT->GetVariable(L"Clover.SoundVolume", gEfiAppleBootGuid, NULL,
                            &OutputVolumeSize, &OutputVolume);
  if (EFI_ERROR(Status)) {
    Status = gRT->GetVariable(BOOT_CHIME_VAR_VOLUME, gBootChimeVendorVariableGuid, NULL,
                              &OutputVolumeSize, &OutputVolume);
    if (EFI_ERROR(Status)) {
      OutputVolume = DefaultAudioVolume; //EFI_AUDIO_IO_PROTOCOL_MAX_VOLUME;
    }
  }
  DefaultAudioVolume = OutputVolume;
  DBG("got volume %d\n", OutputVolume);
  // Success. Assign global variables
  AudioIo = AudioIoProto;
  OldChosenAudio = OutputPortIndex;
  Status = EFI_SUCCESS;

DONE:
  if (StoredDevicePath != NULL)
    FreePool(StoredDevicePath);
  if (AudioIoHandles != NULL)
    FreePool(AudioIoHandles);
  return Status;
}

EFI_STATUS CheckSyncSound(XBool Stop)
{
  EFI_STATUS Status;
  AUDIO_IO_PRIVATE_DATA *AudioIoPrivateData;
  EFI_HDA_IO_PROTOCOL *HdaIo;
  BOOLEAN StreamRunning = false;
  if (!AudioIo) {
    return EFI_NOT_STARTED;
  }

  // Get private data.
  AudioIoPrivateData = AUDIO_IO_PRIVATE_DATA_FROM_THIS(AudioIo);
  if (!AudioIoPrivateData || !AudioIoPrivateData->HdaCodecDev || !AudioIoPrivateData->HdaCodecDev->HdaIo) {
    return EFI_NOT_STARTED;
  }
  HdaIo = AudioIoPrivateData->HdaCodecDev->HdaIo;

  Status = HdaIo->GetStream(HdaIo, EfiHdaIoTypeOutput, &StreamRunning);
  if ((EFI_ERROR(Status) || Stop) && StreamRunning) {
    DBG("stream stopping & controller reset\n");
    HdaIo->StopStream(HdaIo, EfiHdaIoTypeOutput);
//    HDA_IO_PRIVATE_DATA *HdaIoPrivateData = HDA_IO_PRIVATE_DATA_FROM_THIS(HdaIo);
//    HDA_CONTROLLER_DEV *HdaControllerDev = HdaIoPrivateData->HdaControllerDev;
//    EFI_PCI_IO_PROTOCOL *PciIo = HdaControllerDev->PciIo;
//    HdaControllerCleanup(HdaControllerDev);
  }

  if (!StreamRunning) {
    AudioIo = NULL;
    Status = EFI_NOT_STARTED;
  }
  return Status;
}

void GetOutputs()
{
  EFI_STATUS Status;
  // Audio I/O.
  EFI_HANDLE *AudioIoHandles = NULL;
  UINTN AudioIoHandleCount = 0;
  AUDIO_IO_PRIVATE_DATA *AudioIoPrivateData;
  EFI_AUDIO_IO_PROTOCOL *AudioIoTmp = NULL;
  HDA_CODEC_DEV *HdaCodecDev;
  EFI_AUDIO_IO_PROTOCOL_PORT *HdaOutputPorts = NULL;
  UINTN OutputPortsCount = 0;

  UINTN h;

  AudioList.setEmpty();

  // Get Audio I/O protocols.
  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiAudioIoProtocolGuid, NULL, &AudioIoHandleCount, &AudioIoHandles);
  if (EFI_ERROR(Status)) {
    MsgLog("No AudioIoProtocols, status=%s\n", efiStrError(Status));
    return;
  }

  for (h = 0; h < AudioIoHandleCount; h++) {
    UINTN i;
    Status = gBS->HandleProtocol(AudioIoHandles[h], &gEfiAudioIoProtocolGuid, (void**)&AudioIoTmp);
    if (EFI_ERROR(Status)) {
		DBG("dont handle AudioIo at %llu\n", h);
      continue;
    }
    // Get output ports.
    Status = AudioIoTmp->GetOutputs(AudioIoTmp, &HdaOutputPorts, &OutputPortsCount);
    if (EFI_ERROR(Status)) {
      continue;
    }
    if ( OutputPortsCount > 255 ) {
      // SetupPlayback cannot handle index > 255 because parameter is UINT8
      DBG("GetOutputs() : OutputPortsCount > 255.\n");
      OutputPortsCount = 255;
    }
    AudioIoPrivateData = AUDIO_IO_PRIVATE_DATA_FROM_THIS(AudioIoTmp);
    if (!AudioIoPrivateData) {
      continue;
    }
    HdaCodecDev = AudioIoPrivateData->HdaCodecDev;
    for (i = 0; i < OutputPortsCount; i++) {
      HDA_OUTPUTS* hdaOutputPtr = new HDA_OUTPUTS;
      HDA_OUTPUTS& hdaOutput = *hdaOutputPtr;
      //    HdaCodecDev->OutputPorts[i];
      hdaOutput.Name.takeValueFrom(HdaCodecDev->Name);
      hdaOutput.Handle = AudioIoHandles[h];
      hdaOutput.Device = HdaOutputPorts[i].Device;
      hdaOutput.Index = (UINT8)i; // safe cast : OutputPortsCount is <= 255.
      AudioList.AddReference(hdaOutputPtr, true);
    }
  }

  Status = GetStoredOutput();
  if (EFI_ERROR(Status)) {
    DBG("no stored audio parameters\n");
  }
}
