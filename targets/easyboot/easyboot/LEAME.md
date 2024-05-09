Easyboot
========

[Easyboot](https://gitlab.com/bztsrc/easyboot) es un *administrador* de arranque y creador de imágenes de disco de arranque todo en
uno que puede cargar varios núcleos de sistema operativo y núcleos compatibles con Multiboot2 en varios formatos binarios.

![Easyboot](https://gitlab.com/bztsrc/easyboot/raw/main/docs/screenshot.png)

NOTA: Si está buscando un *cargador* de arranque que cargue un solo kernel, eche un vistazo al hermano pequeño de **Easyboot**,
[Simpleboot](https://gitlab.com/bztsrc/simpleboot).

Reconocimiento
--------------

Este proyecto no habría sido posible sin el apoyo de la [Free Software Foundation Hungary](https://fsf.hu/nevjegy). El proyecto
fue creado en el marco de la licitación de software libre 2023 de FSF.hu.

Ventajas sobre GRUB
-------------------

- instalador multilingüe, fácil de usar y sin dependencias
- no está hinchado, es sólo alrededor del 0,5% del tamaño de GRUB
- fácil de configurar, con comprobador de sintaxis
- parchee fácilmente tablas ACPI con DSDT proporcionado por el usuario
- muestra detalles del error si el kernel se activa en una etapa temprana
- Multiboot2: punto de entrada limpio de 64 bits (no es necesario incrustar etiquetas ni código de trampolín de 32 bits en el kernel)
- Multiboot2: soporte para núcleos de mitad superior
- Multiboot2: mapa de memoria consistente e independiente del firmware en todas las plataformas
- Multiboot2: framebuffer consistente e independiente del firmware en todas las plataformas
- Multiboot2: también se proporciona la información EDID del monitor
- Multiboot2: si lo desea, inicia el kernel en todos los núcleos del procesador (soporte SMP)
- también hay un complemento para simular el arranque de GRUB con todos sus errores y dificultades.

Núcleos soportados: [Multiboot1](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html) (ELF32, PE/COFF, a.out;
BIOS, UEFI, RPi), [Multiboot2](docs/es/ABI.md) (ELF32, ELF64, PE32+/COFF, a.out; BIOS, UEFI, RPi),
[Linux](https://www.kernel.org/doc/html/latest/arch/x86/boot.html) (BIOS, UEFI, RPi),
[Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/boot-and-uefi) (UEFI),
[OpenBSD](https://man.openbsd.org/boot.8) (BIOS, UEFI),
[FreeBSD](https://docs.freebsd.org/en/books/handbook/boot/) (BIOS, UEFI),
[FreeDOS](https://www.freedos.org/) (BIOS), [ReactOS](https://reactos.org/) (BIOS),
[MenuetOS](https://menuetos.net/) 32 / 64 (BIOS, UEFI), [KolibriOS](https://kolibrios.org/es/) (BIOS, UEFI),
[SerenityOS](https://serenityos.org/) (BIOS, UEFI), [Haiku](https://www.haiku-os.org/) (BIOS, UEFI)

Sistemas de archivos soportados: [FAT12/16/32](https://social.technet.microsoft.com/wiki/contents/articles/6771.the-fat-file-system.aspx),
[exFAT](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification),
[NTFS](https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20%28NTFS%29.asciidoc) (v3, v3.1),
[ext2/3/4](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout),
[XFS](https://mirror.math.princeton.edu/pub/kernel/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf) (SGI),
[UFS](https://alter.org.ua/docs/fbsd/ufs/) (v2, v4.4),
[mfs](https://gitlab.com/bztsrc/minix3fs) (Minix3),
[FS/Z](https://gitlab.com/bztsrc/bootboot/-/raw/binaries/specs/fsz.pdf),
[BeFS](https://www.haiku-os.org/legacy-docs/practical-file-system-design.pdf),
[AXFS](https://gitlab.com/bztsrc/alexandriafs)

(Cuando el kernel está ubicado en la partición de arranque, se puede usar cualquier sistema de archivos para root: ZFS, btrfs, etc.)

Justificación
-------------

Creé un cargador de arranque fácil de usar y los usuarios pedían más y más funciones. Quería mantener ese gestor de arranque como
lo más simple posible, pero FSF.hu ofreció soporte, así que decidí bifurcarlo y agregar todas las funciones solicitadas en este
administrador de arranque en cambio.

Esta también es una herramienta [sin mamar](https://suckless.org) como Simpleboot, no tiene dependencias y es extremadamente fácil
de usar:

1. cree un directorio y coloque sus archivos de arranque en él, entre otras cosas la configuración de su menú y los complementos opcionales
2. ejecute el comando `easyboot (directorio de origen) (archivo de imagen de salida)`
3. y... eso es todo... ¡no queda nada más por hacer! ¡La imagen *Just Works (TM)*, hará que tus kernels arranquen!

Puede instalar el administrador de arranque y hacer que un dispositivo o imagen existente sea de arranque; o puedes crear una imagen
de arranque nueva. Puede inicie esa imagen en una VM, o puede escribirla con `dd` o [USBImager](https://bztsrc.gitlab.io/usbimager/)
en un almacenamiento y arrancar eso también en una máquina real.

¡Simplicidad es la máxima sofisticación!

Instalación
-----------

Simplemente descargue el binario para su sistema operativo. Estos son ejecutables portátiles, no requieren instalación y no
necesitan ningún bibliotecas compartidas/DLL.

- [easyboot-x86_64-linux.tgz](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-x86_64-linux.tgz) Linux, \*BSD (1M)
- [easyboot-i686-win.zip](https://gitlab.com/bztsrc/easyboot/-/raw/main/distrib/easyboot-i686-win.zip) Windows (1M)

Además, puede encontrar varias soluciones de empaquetado en el directorio [distrib](distrib) (para Debian, Ubuntu, RaspiOS, Gentoo,
Arch).

Al crear una imagen (dependiendo de su configuración), es posible que también necesite algunos complementos en su `(directorio de
origen)`. Tú Puede encontrarlos en el directorio [src/plugins](src/plugins) usando la extensión `plg`. Cuando se instalan desde un
paquete, estos serán colocado en `/usr/share/easyboot`.

Documentación
-------------

La [documentación](docs/es) detallada sobre cómo usar el creador del disco de arranque y cómo se inicia un kernel se puede encontrar
en los docs directorio.

Núcleo de ejemplo
-----------------

Si desea escribir un kernel que pueda cargarse sin ningún complemento usando el protocolo [Multiboot2 simplificado](docs/es/ABI.md)
de **Easyboot**, eche un vistazo a [Kernel de ejemplo de Simpleboot](https://gitlab.com/bztsrc/simpleboot/-/tree/main/example).
Ambos cargadores usan el mismo protocolo de arranque, ese kernel funciona con **Easyboot** tal como está. Verá que no se requiere
Assembly ni etiquetas integradas; de lo contrario, el código fuente es 99,9 % igual que el ejemplo en la especificación Multiboot2
(la única diferencia es que se imprime en la consola serie y no en la pantalla del teletipo VGA, porque eso este último no existe
en máquinas UEFI ni en RaspberryPi).

Compilacion
-----------

Se necesita GNU/make para la orquestación (aunque literalmente es solo `cc easyboot.c -o easyboot`). La cadena de herramientas no
importa, cualquier compilador ANSI C servirá, también funciona en POSIX y WIN32 MINGW. Simplemente vaya al directorio [src](src) y
ejecute `make`. Eso es todo. A pesar de su pequeño tamaño, es autónomo y la compilación tiene exactamente cero dependencias de
biblioteca. No se llama **Easyboot** para nada :-)

Para recompilar los cargadores, necesitará [flatassembler](https://flatassembler.net) y LLVM Clang y lld (gcc y GNU ld no funcionará,
me temo). Pero no te preocupes, los agregué todos a `src/data.h` como una matriz de bytes, para que no tengas que compilar. Estos a
menos que realmente lo desees (para eso, simplemente elimina data.h antes de ejecutar make).

Por otro lado para compilar los complementos, tendrás que tener un compilador cruzado, ya sea LLVM CLang o GNU gcc (x86_64-elf-gcc,
aarch64-elf-gcc). El repositorio también los incluye como binarios. Para realizar la recompilación, simplemente elimine los archivos
`src/plugins/*.plg` antes de ejecutar make.

Licencia
--------

**Easyboot** es software gratuito y de código abierto, con licencia según los términos de GPL versión 3 o (en su opinión) cualquier
versión posterior. Consulte el archivo [LICENSE](LICENSE) para obtener más detalles.

bzt
