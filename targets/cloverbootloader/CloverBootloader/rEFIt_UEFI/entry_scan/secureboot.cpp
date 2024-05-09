/*
 * refit/scan/secureboot.c
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

//CONST INTN SecureBootDef = 0; // jief : not used ??

#ifdef ENABLE_SECURE_BOOT

#include <Platform.h>
#include "../Platform/Settings.h"
#include "../Settings/Self.h"
#include "entry_scan.h"
#include "secureboot.h"

extern "C" {
#include <Protocol/Security.h>
#include <Protocol/Security2.h>
#include <Library/DxeServicesLib.h>
}

#ifndef DEBUG_ALL
#define DEBUG_SECURE_BOOT 1
#else
#define DEBUG_SECURE_BOOT DEBUG_ALL
#endif

#if DEBUG_SECURE_BOOT == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_SECURE_BOOT, __VA_ARGS__)
#endif

// Enable secure boot
void EnableSecureBoot(void)
{
  EFI_STATUS  Status = EFI_NOT_FOUND;
  XBool WantDefaultKeys;
  CONST CHAR16     *ErrorString = NULL;
  UINTN       CloverSignatureSize = 0;
  void       *CloverSignature = NULL;
  // Check in setup mode
  if (GlobalConfig.SecureBoot || !GlobalConfig.SecureBootSetupMode) {
    return;
  }
  // Ask user if they want to use default keys
  WantDefaultKeys = YesNoMessage(L"Secure Boot", L"Enroll the default keys too?");
  DBG("Enabling secure boot with%s default keys\n", WantDefaultKeys ? "" : "out");
  // Get this image's certificate
  UINT32 AuthenticationStatus = 0;
  UINTN  FileSize = 0;
  // Open the file buffer
  void  *FileBuffer = GetFileBufferByFilePath(false, &self.getCloverDirFullDevicePath(), &FileSize, &AuthenticationStatus);
  if (FileBuffer != NULL) {
    if (FileSize > 0) {
      // Retrieve the certificates
      CloverSignature = GetImageSignatureDatabase(FileBuffer, FileSize, &CloverSignatureSize, false);
      if (CloverSignature != NULL) {
        if (CloverSignatureSize > 0) {
          // Found signature
          Status = EFI_SUCCESS;
        } else {
          FreePool(CloverSignature);
          CloverSignature = NULL;
        }
      }
    }
    FreePool(FileBuffer);
  }
  // Check and alert about image not found
  if ((FileBuffer == NULL) || (FileSize == 0)) {
    XStringW FilePath = FileDevicePathToXStringW(&self.getCloverDirFullDevicePath());
    if (FilePath.notEmpty()) {
      DBG("Failed to load Clover image from %ls\n", FilePath.wc_str());
    } else {
      DBG("Failed to load Clover image\n");
    }
  }
  if (EFI_ERROR(Status) || (CloverSignature == NULL)) {
    ErrorString = L"Clover does not have a certificate";
  } else {
    // Enroll secure boot keys
    Status = EnrollSecureBootKeys(CloverSignature, CloverSignatureSize, WantDefaultKeys);
    if (EFI_ERROR(Status)) {
      ErrorString = L"Failed to enroll secure boot keys";
    }
    FreePool(CloverSignature);
  }
  // Reinit secure boot now
  InitializeSecureBoot();
  // Install the security policy hooks or redisable
  if (!EFI_ERROR(Status)) {
    Status = InstallSecureBoot();
    if (EFI_ERROR(Status)) {
      ErrorString = L"Secure boot protocols not found";
    }
  }
  if (EFI_ERROR(Status)) {
    XStringW Str = SWPrintf("Enabling secure boot failed because\n%ls", ErrorString);
    AlertMessage(L"Enable Secure Boot"_XSW, Str);
    DBG("Enabling secure boot failed because %ls! Status: %s\n", ErrorString, efiStrError(Status));
    DisableSecureBoot();
  }
}

CONST CHAR16 *SecureBootPolicyToStr(IN UINTN Policy)
{
  STATIC CONST CHAR16 *SecureBootPolicyStrings[] = {
    L"Deny",
    L"Allow",
    L"Query",
    L"Insert",
    L"WhiteList",
    L"BlackList",
    L"User",
  };
  STATIC CONST UINTN  SecureBootPolicyStringsCount = (sizeof(SecureBootPolicyStrings) / (sizeof(CONST CHAR16 *)));
  if (Policy < SecureBootPolicyStringsCount) {
    return SecureBootPolicyStrings[Policy];
  }
  return L"Deny";
}

STATIC void PrintSecureBootInfo(void)
{
  // Nothing to do if secure boot is disabled or in setup mode
  if (!GlobalConfig.SecureBoot) {
    DBG("Secure Boot: %s\n", (GlobalConfig.SecureBootSetupMode ? "Setup" : "Disabled"));
  } else {
    // Secure boot is enabled
    DBG("Secure Boot: %s\n", (GlobalConfig.SecureBootSetupMode ? "Forced" : "Enabled"));
    DBG("Boot Policy: %ls\n", SecureBootPolicyToStr(gSettings.Boot.SecureBootPolicy));
  }
}

// Alert message for disable failure
STATIC void DisableMessage(const EFI_STATUS  Status,
                           const CHAR16     *String,
                           const CHAR16     *ErrorString)
{
  XStringW Str;
  if (ErrorString != NULL) {
    Str = SWPrintf("%ls\n%ls\n%s", String, ErrorString, efiStrError(Status));
  } else {
    Str = SWPrintf("%ls\n%s", String, efiStrError(Status));
  }
  DBG("Secure Boot: %ls", Str.wc_str());
  AlertMessage(L"Disable Secure Boot"_XSW, Str);
}

// Disable secure boot
void DisableSecureBoot(void)
{
  EFI_STATUS  Status;
  const CHAR16     *ErrorString = NULL;
  // Check in user mode
  if (GlobalConfig.SecureBootSetupMode || !GlobalConfig.SecureBoot) {
    return;
  }
  UninstallSecureBoot();
  // Clear the platform database
  Status = ClearSecureBootKeys();
  if (EFI_ERROR(Status)) {
    ErrorString = L"Failed to clear platform database!\nIs Clover platform database owner?";
  } else if (YesNoMessage(L"Disable Secure Boot", L"Do you want to clear all databases (suggested)?")) {
    // Clear all databases if wanted
    Status = SetSignatureDatabase(EXCHANGE_DATABASE_NAME, EXCHANGE_DATABASE_GUID, NULL, 0);
    if (EFI_ERROR(Status)) {
      DisableMessage(Status, L"Failed to disable secure boot!", L"Failed to clear exchange database!");
    }
    Status = ClearAuthorizedDatabase();
    if (EFI_ERROR(Status)) {
      ErrorString = L"Failed to clear authorized database!";
    }
  }
  // Alert failure to user
  if (EFI_ERROR(Status)) {
    DisableMessage(Status, L"Failed to disable secure boot!", ErrorString);
  } else {
    // Reinit secure boot now
    InitializeSecureBoot();
    PrintSecureBootInfo();
  }
}

// The previous protocol functions
STATIC EFI_SECURITY_FILE_AUTHENTICATION_STATE gSecurityFileAuthentication;
STATIC EFI_SECURITY2_FILE_AUTHENTICATION      gSecurity2FileAuthentication;

// Pre check the secure boot policy
STATIC XBool EFIAPI
PrecheckSecureBootPolicy(IN OUT EFI_STATUS                     *AuthenticationStatus,
                         IN     CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
  XStringW DevicePathStr;
  UINTN   Index;
  if ((AuthenticationStatus == NULL) || (DevicePath == NULL)) {
    return false;
  }
  switch (gSettings.Boot.SecureBootPolicy) {
  case SECURE_BOOT_POLICY_ALLOW:
    // Allow all images
    *AuthenticationStatus = EFI_SUCCESS;
    return true;

  case SECURE_BOOT_POLICY_WHITELIST:
    // Check the white list for this image
    DevicePathStr = FileDevicePathToXStringW((EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
    if ( DevicePathStr.isEmpty() ) {
      return false;
    }
    for (Index = 0; Index < gSettings.Boot.SecureBootWhiteList.size(); ++Index) {
      if ( gSettings.Boot.SecureBootWhiteList[Index].notEmpty() &&
          DevicePathStr.contains(gSettings.Boot.SecureBootWhiteList[Index]) ) {
        // White listed
        *AuthenticationStatus = EFI_SUCCESS;
        return true;
      }
    }
    return true;

  case SECURE_BOOT_POLICY_BLACKLIST:
    // Check the black list for this image
    DevicePathStr = FileDevicePathToXStringW((EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
    if ( DevicePathStr.isEmpty() ) {
      return false;
    }
    for (Index = 0; Index < gSettings.Boot.SecureBootBlackList.size(); ++Index) {
      if ( gSettings.Boot.SecureBootBlackList[Index].notEmpty() &&
          DevicePathStr.contains(gSettings.Boot.SecureBootBlackList[Index]) ) {
        // Black listed
        return true;
      }
    }
    *AuthenticationStatus = EFI_SUCCESS;
    return true;

  case SECURE_BOOT_POLICY_USER:
    DevicePathStr = FileDevicePathToXStringW((EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
    if ( DevicePathStr.isEmpty() ) {
      return false;
    }
    // Check the black list for this image
    for (Index = 0; Index < gSettings.Boot.SecureBootBlackList.size(); ++Index) {
      if ( gSettings.Boot.SecureBootBlackList[Index].notEmpty() &&
          DevicePathStr.contains(gSettings.Boot.SecureBootBlackList[Index]) ) {
        // Black listed
        return true;
      }
    }
    // Check the white list for this image
    for (Index = 0; Index < gSettings.Boot.SecureBootWhiteList.size(); ++Index) {
      if ( gSettings.Boot.SecureBootWhiteList[Index].notEmpty() &&
          DevicePathStr.contains(gSettings.Boot.SecureBootWhiteList[Index]) ) {
        // White listed
        *AuthenticationStatus = EFI_SUCCESS;
        return true;
      }
    }
    break;
  }
  return false;
}

// Check the secure boot policy
STATIC XBool EFIAPI
CheckSecureBootPolicy(IN OUT EFI_STATUS                     *AuthenticationStatus,
                      IN     CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
                      IN     void                           *FileBuffer,
                      IN     UINTN                           FileSize)
{
  UINTN UserResponse = SECURE_BOOT_POLICY_DENY;
  switch (gSettings.Boot.SecureBootPolicy) {
  case SECURE_BOOT_POLICY_QUERY:
  case SECURE_BOOT_POLICY_USER:
    // Query user to allow image or deny image or insert image signature
    UserResponse = QuerySecureBootUser(DevicePath);
    DBG("VerifySecureBootImage: User selected policy: %ls\n", SecureBootPolicyToStr(UserResponse));
    // Perform user action
    switch (UserResponse) {
    case SECURE_BOOT_POLICY_ALLOW:
      *AuthenticationStatus = EFI_SUCCESS;

    case SECURE_BOOT_POLICY_DENY:
    default:
      return true;

    case SECURE_BOOT_POLICY_INSERT:
      // If this is forced mode then no insert
      if (GlobalConfig.SecureBootSetupMode) {
        return true;
      }
      break;
    }
    // Purposeful fallback to insert

  case SECURE_BOOT_POLICY_INSERT:
    // Insert image signature
    AppendImageToAuthorizedDatabase(DevicePath, FileBuffer, FileSize);
    *AuthenticationStatus = EFI_SUCCESS;
    return true;

  case SECURE_BOOT_POLICY_ALLOW:
    // Allow all images
    *AuthenticationStatus = EFI_SUCCESS;
    return true;

  case SECURE_BOOT_POLICY_DENY:
     // Deny all images
     return true;
  }
  return false;
}


// Override EFI_SECURITY_ARCH_PROTOCOL
EFI_STATUS EFIAPI
InternalFileAuthentication(IN CONST EFI_SECURITY_ARCH_PROTOCOL *This,
                           IN UINT32                            AuthenticationStatus,
                           IN CONST EFI_DEVICE_PATH_PROTOCOL   *DevicePath)
{
  EFI_STATUS Status = EFI_SECURITY_VIOLATION;
  // Check secure boot policy
  if (!PrecheckSecureBootPolicy(&Status, DevicePath)) {
    // Return original security policy
    Status = gSecurityFileAuthentication(This, AuthenticationStatus, DevicePath);
    if (EFI_ERROR(Status)) {
      // Check security policy on image
      CheckSecureBootPolicy(&Status, DevicePath, NULL, 0);
    }
  }
  if (EFI_ERROR(Status)) {
    XStringW DevicePathStr = FileDevicePathToXStringW((EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
    if ( DevicePathStr.notEmpty() ) {
      DBG("VerifySecureBootImage(1): %s %ls\n", efiStrError(Status), DevicePathStr.wc_str());
    }
  }
  return Status;
}

// Override EFI_SECURITY2_ARCH_PROTOCOL
EFI_STATUS EFIAPI
Internal2FileAuthentication(IN CONST EFI_SECURITY2_ARCH_PROTOCOL *This,
                            IN CONST EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
                            IN void                              *FileBuffer,
                            IN UINTN                              FileSize,
                            IN BOOLEAN                            BootPolicy)
{
  EFI_STATUS Status = EFI_SECURITY_VIOLATION;
  // Check secure boot policy
  if (!PrecheckSecureBootPolicy(&Status, DevicePath)) {
    // Return original security policy
    Status = gSecurity2FileAuthentication(This, DevicePath, FileBuffer, FileSize, BootPolicy);
    if (EFI_ERROR(Status)) {
      CheckSecureBootPolicy(&Status, DevicePath, FileBuffer, FileSize);
    }
  }
  if (EFI_ERROR(Status)) {
    XStringW DevicePathStr = FileDevicePathToXStringW((EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
    if ( DevicePathStr.notEmpty() ) {
      DBG("VerifySecureBootImage(2): %s %ls\n", efiStrError(Status), DevicePathStr.wc_str());
    }
  }
  return Status;
}

// Verify boot policy for image
EFI_STATUS VerifySecureBootImage(IN CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath)
{
  EFI_STATUS Status = EFI_SECURITY_VIOLATION;
  if (!PrecheckSecureBootPolicy(&Status, DevicePath)) {
    if (!CheckSecureBootPolicy(&Status, DevicePath, NULL, 0)) {
      Status = EFI_SUCCESS;
    }
  }
  if (EFI_ERROR(Status)) {
    XStringW DevicePathStr = FileDevicePathToXStringW((EFI_DEVICE_PATH_PROTOCOL *)DevicePath);
    if ( DevicePathStr.notEmpty() ) {
      DBG("VerifySecureBootImage: %s %ls\n", efiStrError(Status), DevicePathStr.wc_str());
    }
  }
  return Status;
}

// Install secure boot
EFI_STATUS InstallSecureBoot(void)
{
  EFI_STATUS                   Status;
  EFI_SECURITY_ARCH_PROTOCOL  *Security = NULL;
  EFI_SECURITY2_ARCH_PROTOCOL *Security2 = NULL;
  // Check if already installed
  if (gSecurityFileAuthentication) {
    return EFI_SUCCESS;
  }
  PrintSecureBootInfo();
  // Nothing to do if secure boot is disabled or in setup mode
  if (!GlobalConfig.SecureBoot || GlobalConfig.SecureBootSetupMode) {
    return EFI_SUCCESS;
  }
  // Locate security protocols
  gBS->LocateProtocol(gEfiSecurity2ArchProtocolGuid, NULL, (void **)&Security2);
  Status = gBS->LocateProtocol(gEfiSecurityArchProtocolGuid, NULL, (void **)&Security);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  if (Security == NULL) {
    return EFI_NOT_FOUND;
  }
  // Install policy hooks
  gSecurityFileAuthentication = Security->FileAuthenticationState;
  Security->FileAuthenticationState = InternalFileAuthentication;
  if (Security2) {
    gSecurity2FileAuthentication = Security2->FileAuthentication;
    Security2->FileAuthentication = Internal2FileAuthentication;
  }
  return EFI_SUCCESS;
}

// Uninstall secure boot
void UninstallSecureBoot(void)
{
  // Uninstall policy hooks
  if (gSecurityFileAuthentication) {
    EFI_SECURITY_ARCH_PROTOCOL  *Security = NULL;
    // Restore the security protocol function
    gBS->LocateProtocol(gEfiSecurityArchProtocolGuid, NULL, (void **)&Security);
    if (Security) {
      Security->FileAuthenticationState = gSecurityFileAuthentication;
    }
    gSecurityFileAuthentication = 0;
  }
  if (gSecurity2FileAuthentication) {
    EFI_SECURITY2_ARCH_PROTOCOL *Security2 = NULL;
    // Restory the security 2 protocol function
    gBS->LocateProtocol(gEfiSecurity2ArchProtocolGuid, NULL, (void **)&Security2);
    if (Security2) {
      Security2->FileAuthentication = gSecurity2FileAuthentication;
    }
    gSecurity2FileAuthentication = 0;
  }
}

// Initialize secure boot
void InitializeSecureBoot(void)
{
  // Set secure boot variables to firmware values
  UINTN Size = sizeof(GlobalConfig.SecureBootSetupMode);
  gRT->GetVariable(L"SetupMode", gEfiGlobalVariableGuid, NULL, &Size, &GlobalConfig.SecureBootSetupMode);
  Size = sizeof(GlobalConfig.SecureBoot);
  gRT->GetVariable(L"SecureBoot", gEfiGlobalVariableGuid, NULL, &Size, &GlobalConfig.SecureBoot);
  // Make sure that secure boot is disabled if in setup mode, this will
  //  allow us to specify later in settings that we want to override
  //  setup mode and pretend like we are in secure boot mode to enforce
  //  secure boot policy even when secure boot is not present/disabled.
  if (GlobalConfig.SecureBootSetupMode) {
    GlobalConfig.SecureBoot = 0;
  }
}

#endif // ENABLE_SECURE_BOOT
