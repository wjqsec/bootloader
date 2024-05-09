Easyboot with UEFI Secure Boot
==============================

First of all, avoid whenever you can, as this is a scam, only secure in its name (see CVE-2016-332, CVE-2016-3287, CVE-2020-14372,
CVE-2020-20233, CVE-2020-25632, CVE-2020-25647, CVE-2020-27779, CVE-2020-27749, CVE-2021-3418, CVE-2021-20233, CVE-2021-20225,
CVE-2022-21894, CVE-2022-34301, CVE-2022-34302, CVE-2022-34303, etc).

If you can't avoid it for whatever reason, here are the steps.

Prerequisites
-------------

1. Download the latest [shim binary](https://kojipkgs.fedoraproject.org/packages/shim) and extract `shimx64.efi` and `mmx64.efi`
   files from the RPM.
2. Install **Easyboot** on your disk as usual.

Setting Up Secure Boot
----------------------

1. Rename `EFI\BOOT\BOOTX64.EFI` to `EFI\BOOT\GRUBX64.EFI` on the ESP partition.
2. Copy `shimx64.efi` to the ESP as `EFI\BOOT\BOOTX64.EFI`.
3. Copy `mmx64.efi` to the ESP as `EFI\BOOT\MMX64.EFI`.
4. Reboot, and enter UEFI configuration menu.
5. Check if boot order has `EFI\BOOT\BOOTX64.EFI`, add it if it's missing.
6. Enable Secure Boot and exit configuration.
7. Shim will start MokManager.
8. Select `Enroll hash from disk`.
9. Specify `EFI\BOOT\GRUBX64.EFI` and add it to the MokList.
10. Select `Continue boot`.

After this (and on every subsequent reboots) **Easyboot** should start in Secure Boot mode.

