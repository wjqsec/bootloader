Easyboot mit UEFI Secure Boot
=============================

Vermeiden Sie es zunächst, wann immer Sie können, denn es handelt sich um einen Betrug, der nur seinem Namen nach sicher ist
(ablesen CVE-2016-332, CVE-2016-3287, CVE-2020-14372, CVE-2020-20233, CVE-2020-25632, CVE-2020-25647, CVE-2020-27779, CVE-2020-27749,
CVE-2021-3418, CVE-2021-20233, CVE-2021-20225, CVE-2022-21894, CVE-2022-34301, CVE-2022-34302, CVE-2022-34303, usw).

Wenn Sie es aus irgendeinem Grund nicht vermeiden können, finden Sie hier die Schritte.

Voraussetzungen
---------------

1. Laden Sie die neueste [Shim-Binärdatei](https://kojipkgs.fedoraproject.org/packages/shim) herunter und extrahieren Sie die
   Dateien `shimx64.efi` und `mmx64.efi` aus dem RPM.
2. Installieren Sie **Easyboot** wie gewohnt auf Ihrer Festplatte.

Secure Boot einrichten
----------------------

1. Benennen Sie `EFI\BOOT\BOOTX64.EFI` auf der ESP-Partition in `EFI\BOOT\GRUBX64.EFI` um.
2. Kopieren Sie `shimx64.efi` als `EFI\BOOT\BOOTX64.EFI` in den ESP.
3. Kopieren Sie `mmx64.efi` als `EFI\BOOT\MMX64.EFI` in den ESP.
4. Starten Sie neu und rufen Sie das UEFI-Konfigurationsmenü auf.
5. Überprüfen Sie, ob die boot order `EFI\BOOT\BOOTX64.EFI` enthält, und fügen Sie sie hinzu, falls sie fehlt.
6. Aktivieren Sie Secure Boot und beenden Sie die Konfiguration.
7. Shim startet MokManager.
8. Wählen Sie `Enroll hash from disk`.
9. Geben Sie `EFI\BOOT\GRUBX64.EFI` an und fügen Sie es der MokList hinzu.
10. Wählen Sie `Continue boot`.

Danach (und bei jedem weiteren Neustart) sollte **Easyboot** im Secure Boot-Modus starten.
