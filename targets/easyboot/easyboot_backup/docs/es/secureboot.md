Easyboot con UEFI Secure Boot
=============================

En primer lugar, evítelo siempre que pueda, ya que se trata de una estafa, solo segura en su nombre (consulte CVE-2016-332,
CVE-2016-3287, CVE-2020-14372, CVE-2020-20233, CVE-2020-25632, CVE-2020-25647, CVE-2020-27779, CVE-2020-27749, CVE-2021-3418,
CVE-2021-20233, CVE-2021-20225, CVE-2022-21894, CVE-2022-34301, CVE-2022-34302, CVE-2022-34303, etc).

Si no puedes evitarlo por cualquier motivo, aquí tienes los pasos.

Requisitos previos
------------------

1. Descargue el [binario shim](https://kojipkgs.fedoraproject.org/packages/shim) más reciente y extraiga los archivos `shimx64.efi`
   y `mmx64.efi` del RPM.
2. Instale **Easyboot** en su disco como de costumbre.

Configurar el Secure Boot
-------------------------

1. Cambie el nombre de `EFI\BOOT\BOOTX64.EFI` a `EFI\BOOT\GRUBX64.EFI` en la partición ESP.
2. Copie `shimx64.efi` al ESP como `EFI\BOOT\BOOTX64.EFI`.
3. Copie `mmx64.efi` al ESP como `EFI\BOOT\MMX64.EFI`.
4. Reinicie e ingrese al menú de configuración UEFI.
5. Verifique si el boot order tiene `EFI\BOOT\BOOTX64.EFI`, agréguelo si falta.
6. Habilite el Secure Boot y salga de la configuración.
7. Shim iniciará MokManager.
8. Seleccione `Enroll hash from disk`.
9. Especifique `EFI\BOOT\GRUBX64.EFI` y agréguelo a MokList.
10. Seleccione `Continue boot`.

Después de esto (y en cada reinicio posterior), **Easyboot** debería iniciarse en modo de Secure Boot.
