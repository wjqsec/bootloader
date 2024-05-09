Easyboot UEFI Secure Boot-al
============================

Először is, kerüld, amikor csak lehet, mert ez egy átverés, csak a nevében biztonságos (lásd CVE-2016-332, CVE-2016-3287,
CVE-2020-14372, CVE-2020-20233, CVE-2020-25632, CVE-2020-25647, CVE-2020-27779, CVE-2020-27749, CVE-2021-3418, CVE-2021-20233,
CVE-2021-20225, CVE-2022-21894, CVE-2022-34301, CVE-2022-34302, CVE-2022-34303, stb).

Ha mégsem tudnád elkerülni valamiért, akkor ezek a szükséges lépések.

Előkészületek
-------------

1. Töltsd le a legfrissebb [shim bináris](https://kojipkgs.fedoraproject.org/packages/shim)t és csomagold ki a `shimx64.efi`
   és az `mmx64.efi` fájlokat az RPM-ből.
2. Telepítsd az **Easyboot**-ot a szokásos módon a lemezedre.

Secure Boot beállítása
----------------------

1. Az ESP partíción nevezd át az `EFI\BOOT\BOOTX64.EFI`-t `EFI\BOOT\GRUBX64.EFI`-re.
2. Másold át a `shimx64.efi`-t az ESP-re `EFI\BOOT\BOOTX64.EFI` néven.
3. Másold át az `mmx64.efi`-t az ESP-re `EFI\BOOT\MMX64.EFI` néven.
4. Indítsd újra a gépet, és lépj be az UEFI konfiguráló menüjébe.
5. Ellenőrizd, hogy a boot order-ben szerepel-e az `EFI\BOOT\BOOTX64.EFI` és add hozzá, ha hiányozna.
6. Engedélyezd a Secure Boot-ot és lépj ki a konfigurálóból.
7. A shim a MokManager-t fogja indítani.
8. Válaszd az `Enroll hash from disk` opciót.
9. Add meg az `EFI\BOOT\GRUBX64.EFI`-t és add hozzá a MokList-hez.
10. Válaszd a `Continue boot` opciót.

Ezt követően (és minden további újraindítás után) az **Easyboot** már Secure Boot módban kell, hogy induljon.
