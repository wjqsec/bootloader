Easyboot avec UEFI Secure Boot
==============================

Tout d'abord, évitez autant que possible, car il s'agit d'une arnaque, sécurisée uniquement en son nom (voir CVE-2016-332,
CVE-2016-3287, CVE-2020-14372, CVE-2020-20233, CVE-2020-25632, CVE-2020-25647, CVE-2020-27779, CVE-2020-27749, CVE-2021-3418,
CVE-2021-20233, CVE-2021-20225, CVE-2022-21894, CVE-2022-34301, CVE-2022-34302, CVE-2022-34303, etc).

Si vous ne pouvez pas l’éviter pour une raison quelconque, voici les étapes à suivre.

Conditions préalables
---------------------

1. Téléchargez le dernier [shim binaire](https://kojipkgs.fedoraproject.org/packages/shim) et extrayez les fichiers `shimx64.efi`
   et `mmx64.efi` du RPM.
2. Installez **Easyboot** sur votre disque comme d'habitude.

Configuration du Secure Boot
----------------------------

1. Renommez `EFI\BOOT\BOOTX64.EFI` en `EFI\BOOT\GRUBX64.EFI` sur la partition ESP.
2. Copiez `shimx64.efi` dans l'ESP sous le nom `EFI\BOOT\BOOTX64.EFI`.
3. Copiez `mmx64.efi` dans l'ESP sous le nom `EFI\BOOT\MMX64.EFI`.
4. Redémarrez et entrez dans le menu de configuration UEFI.
5. Vérifiez si l'ordre de démarrage contient `EFI\BOOT\BOOTX64.EFI`, ajoutez-le s'il est manquant.
6. Activez le démarrage sécurisé et quittez la configuration.
7. Shim lancera MokManager.
8. Sélectionnez `Enroll hash from disk`.
9. Spécifiez `EFI\BOOT\GRUBX64.EFI` et ajoutez-le à la MokList.
10. Sélectionnez `Continue boot`.

Après cela (et à chaque redémarrage ultérieur), **Easyboot** devrait démarrer en mode Secure Boot.
