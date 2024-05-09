Arrancando kernels con Easyboot
===============================

[Easyboot](https://gitlab.com/bztsrc/easyboot) es un administrador de arranque todo en uno y un creador de imágenes de disco de
arranque que puede cargar varios núcleos de sistema operativo y núcleos compatibles con Multiboot2 en varios formatos.

[[_TOC_]]

Instalación
-----------

```
 easyboot [-v|-vv] [-s <mb>] [-b <mb>] [-u <guid>] [-p <t> <u> <i>] [-e] [-c] <indir> <outfile>

  -v, -vv         aumentar la verbosidad / validación
  -s <mb>         establezca el tamaño de la imagen del disco en Megabytes (el valor predeterminado es 35 M)
  -b <mb>         establezca el tamaño de la partición de arranque en Megabytes (el valor predeterminado es 33 M)
  -u <guid>       establecer el identificador único de la partición de arranque (el valor predeterminado es aleatorio)
  -p <t> <u> <i>  agregar una partición extra (tipo guid, único guid, archivo de imagen)
  -e              agregue El Torito Boot Catalog (soporte de arranque de CDROM EFI)
  -c              siempre cree un nuevo archivo de imagen incluso si existe
  <indir>         use el contenido de este directorio para la partición de arranque
  <outfile>       imagen de salida o nombre de archivo del dispositivo
```

La herramienta **Easyboot** crea una imagen de disco de arranque llamada `(outfile)` usando la tabla de particiones GUID con una
única partición formateada como FAT32 y denominada "Partición del sistema EFI" (ESP para abreviar). El contenido de esa partición
se toma del `(indir)` que usted proporciona. Debe colocar un archivo de configuración de texto plano simple en ese directorio
llamado `easyboot/menu.cfg`. Con finales de línea NL o CRLF, puedes editarlo fácilmente con cualquier editor de texto. Dependiendo
de su configuración, es posible que también necesite algunos [complementos](plugins.md) en este directorio llamado `easyboot/*.plg`.

La imagen también se puede iniciar en Raspberry Pi y funcionará en qemu; pero para arrancar en una máquina real, necesitará más
archivos de firmware `bootcode.bin`, `fixup.dat`, `start.elf` y un archivo .dtb en el directorio `(indir)`, estos se pueden descargar
del [repositorio oficial](https://github.com/raspberrypi/firmware/tree/master/boot) de Raspberry Pi.

La herramienta también tiene algunos indicadores de línea de comando opcionales: `-s (mb)` establece el tamaño total de la imagen de
disco generada en Megabytes, mientras que `-b (mb)` establece el tamaño de la partición de inicio en Megabytes. Evidentemente el
primero debe ser mayor que el segundo. Si no se especifica, el tamaño de la partición se calcula a partir del tamaño del directorio
dado (33 Mb como mínimo, el FAT32 más pequeño que puede haber) y el tamaño del disco por defecto es 2 Mb mayor que eso (debido a la
alineación y al espacio necesario para la partición). mesa). Si hay un espacio de más de 2 Mb entre estos dos valores de tamaño,
entonces puede usar herramientas de terceros como `fdisk` para agregar más particiones a la imagen a su gusto (o vea `-p` a
continuación). Si desea un diseño predecible, también puede especificar el identificador único de la partición de inicio
(UniquePartitionGUID) con el indicador `-u <guid>`.

Opcionalmente, también puede agregar particiones adicionales con el indicador `-p`. Esto requiere 3 argumentos: (PartitionTypeGUID),
(UniquePartitionGUID) y el nombre del archivo de imagen que contiene el contenido de la partición.

El indicador `-e` agrega El Torito Boot Catalog a la imagen generada, de modo que se pueda iniciar no solo como una memoria USB
sino también como un CDROM EFI.

Si `(outfile)` es un archivo de dispositivo (por ejemplo, `/dev/sda` en Linux, `/dev/disk0` en BSD y `\\.\PhysicalDrive0` en Windows),
entonces no crea GPT ni ESP, en cambio localiza los ya existentes en el dispositivo. Todavía copia todos los archivos en `(indir)` a
la partición de arranque e instala los cargadores. Esto también funciona si `(outfile)` es un archivo de imagen que ya existe (en
este caso use el indicador `-c` para crear siempre primero un archivo de imagen nuevo y vacío).

Configuración
-------------

El archivo `easyboot/menu.cfg` puede contener las siguientes líneas (muy similar a la sintaxis de grub.cfg, puede encontrar un
archivo de configuración de ejemplo [aquí](menu.cfg)):

### Comentarios

Todas las líneas que comienzan con `#` se consideran comentarios y se omiten hasta el final de la línea.

### Nivel de verbosidad

Puede establecer el nivel de detalle usando una línea que comience con `verbose`.

```
verbose (0-3)
```

Esto le dice al cargador cuánta información imprimir en la consola de arranque. Detallado 0 significa totalmente silencioso
(predeterminado) y detallado 3 volcará los segmentos del kernel cargados y el código de máquina en el punto de entrada.

### Framebuffer

Puede solicitar una resolución de pantalla específica con la línea que comienza con `framebuffer`. El formato es el siguiente:

```
framebuffer (ancho) (alto) (bits por píxel) [#(primer plano)] [#(fondo)] [#(barra de progreso)]
```

**Easyboot** configurará un framebuffer para usted, incluso si esta línea no existe (800 x 600 x 32 bpp de forma predeterminada).
Pero si esta línea existe, intentará establecer la resolución especificada. Los modos de paleta no son compatibles, por lo que los
bits por píxel deben ser al menos 15.

Si se proporciona el cuarto parámetro de color opcional, entonces tiene que estar en notación HTML comenzando con una marca hash
seguida de 6 dígitos hexadecimales, RRGGBB. Por ejemplo, `#ff0000` es de color rojo brillante y `#007f7f` es de un cian más oscuro.
Establece el color de primer plano o, en otras palabras, el color de la fuente. Del mismo modo, si se proporciona el quinto parámetro
de color opcional, también debe estar en notación HTML y establece el color de fondo. El último parámetro de color opcional es
similar y establece el color de fondo de la barra de progreso.

Cuando no se proporcionan colores, el valor predeterminado son letras blancas sobre fondo negro y el fondo de la barra de progreso
es gris oscuro.

### Opción de arranque predeterminada

La línea que comienza con `default` establece qué entrada del menú debe iniciarse sin interacción del usuario después del tiempo de
espera especificado.

```
default (índice de menú) (tiempo de espera mseg)
```

El índice de entrada de menú es un número entre 1 y 9. El tiempo de espera está en milisegundos, por lo que 1000 equivale a un segundo.

### Alineación del menú

Las líneas que comienzan con `menualign` alteran la forma en que se muestran las opciones de inicio.

```
menualign ("vertical"|"horizontal")
```

Por defecto el menú es horizontal, que puedes cambiar a vertical.

### Opciones de arranque

Puede especificar hasta 9 entradas de menú con líneas que comiencen con `menuentry`. El formato es el siguiente:

```
menuentry (icono) [etiqueta]
```

Para cada ícono, debe existir un archivo `easyboot/(icono).tga` en la partición de inicio. La imagen debe estar codificada en una
longitud de tirada y asignada en color [formato Targa](../en/TGA.txt), porque es la variante más comprimida (los primeros tres bytes
del archivo deben ser `0`, `1` y `9` en este orden, consulte el tipo de datos 9 en la especificación). Sus dimensiones deben ser
exactamente 64 píxeles de alto y 64 píxeles de ancho.

Para guardar en este formato desde GIMP, primero seleccione "Imagen > Modo > Indexado...", en la ventana emergente establezca
"Número máximo de colores" en 256. Luego seleccione "Archivo > Exportar como...", ingrese un nombre de archivo que termine en `.tga`,
y en la ventana emergente marque "Compresión RLE". Para una herramienta de conversión de línea de comando, puede usar ImageMagick,
`convert (cualquier archivo de imagen) -colors 256 -compress RLE icono.tga`.

El parámetro de etiqueta opcional es para mostrar la versión ASCII o información de lanzamiento en el menú, y no para cadenas
arbitrarias; por lo tanto, para ahorrar espacio, no se admite UTF-8 a menos que también proporcione `easyboot/font.sfn`. (Una fuente
UNICODE requiere mucho almacenamiento, aunque la [Scalable Screen Font](https://gitlab.com/bztsrc/scalable-font2) es muy eficiente,
todavía pesa alrededor de 820 K. En contraste, unicode.pf2 de GRUB es mucho más grande. (más grande, alrededor de 2392 K, aunque
ambas fuentes contienen aproximadamente 55600 glifos en mapas de bits de 8 x 16 y 16 x 16. La fuente ASCII integrada tiene mapas de
bits de 8 x 16 y 96 glifos únicamente).

Todas las líneas que vienen después de una línea de `menuentry` pertenecerán a esa entrada de menú, excepto cuando esa línea sea
otra línea de entrada de menú. Para mayor comodidad, puedes usar bloques como en GRUB, pero estos son solo azúcar sintáctico. Las
llaves se manejan como caracteres de espacio en blanco. Puede omitirlos y usar pestañas en su lugar como en un script de Python o
Makefile si así lo prefiere.

Por ejemplo
```
menuentry FreeBSD backup
{
    kernel bsd.old/boot
}
```

### Seleccionar partición

La línea que comienza con `partition` selecciona una partición GPT. Debe estar precedido por una línea de `menuentry`.

```
partition (partición UUID única)
```

Esta partición se utilizará como sistema de archivos raíz para la opción de inicio. El kernel y los módulos se cargarán desde esta
partición y, según el protocolo de arranque, también se pasarán al kernel. La partición especificada puede residir en un disco
diferente al disco de inicio, **Easyboot** iterará en todos los discos particionados GPT durante el inicio para localizarla.

Por conveniencia, la partición también se busca en la línea `kernel` (ver más abajo). Si la línea de comando de arranque dada
contiene una cadena `root=(UUID)` o `root=*=(UUID)`, entonces no hay necesidad de una línea de `partición` separada.

Cuando la partición no se especifica explícitamente, el kernel y los módulos se cargan desde la partición de inicio.

### Especificar un kernel

La línea que comienza con `kernel` indica qué archivo se debe iniciar y con qué parámetros. Debe estar precedido por una línea de
`menuentry`.

```
kernel (ruta a su archivo de kernel) (argumentos de línea de comando de arranque opcionales)
```

La ruta debe apuntar a un archivo existente, un binario ejecutable del kernel, y tiene que ser una ruta UTF-8 absoluta en la
partición raíz (o de inicio). Si el kernel no está en el directorio raíz de la partición, entonces el separador de directorio
siempre es `/`, incluso en sistemas UEFI. Si el nombre contiene un espacio, se debe utilizar como escape `\`. La ruta puede ir
seguida de argumentos de la línea de comando, separados por un espacio. Para los núcleos compatibles con Multiboot2, estos
argumentos de la línea de comando se pasarán en la etiqueta *Línea de comando de arranque* (tipo 1). **Easyboot** no los modificará
ni los analizará, excepto que se buscará el especificador de partición.

De forma predeterminada, **Easyboot** puede iniciar núcleos compatibles con Multiboot2 en formatos ELF64 y PE32+/COFF (y en sistemas
UEFI, también aplicaciones UEFI). Normalmente, ese protocolo no permite núcleos de mitad superior, pero **Easyboot** viola un poco
el protocolo de una manera que no rompe los núcleos normales que no son de mitad superior. Si desea iniciar cualquier otro kernel,
necesitará un cargador de kernel [complemento](plugins.md).

NOTA: a diferencia de GRUB, donde debe usar comandos especiales como "linux" o "multiboot" para seleccionar el protocolo de arranque,
aquí solo hay un comando y el protocolo se detecta automáticamente desde su kernel en tiempo de ejecución.

### Cargando más módulos

Puede cargar archivos arbitrarios (discos ram iniciales, controladores del kernel, etc.) junto con el kernel usando líneas que
comienzan con `module`. Debe estar precedido por una línea de `menuentry`. Tenga en cuenta que esta línea se puede repetir varias
veces dentro de cada entrada del menú. Si el protocolo de arranque admite un initrd, entonces la primera línea del `module` se
considera como initrd.

```
module (ruta a un archivo) (argumentos de línea de comando del módulo opcional)
```

La ruta debe apuntar a un archivo existente y debe ser una ruta UTF-8 absoluta en la partición raíz (o de inicio). Puede ir seguido
de argumentos de línea de comando, separados por un espacio. Si el archivo está comprimido y hay un [complemento](plugins.md) de
descompresión para él, entonces el módulo se descomprimirá de forma transparente. La información sobre estos módulos cargados (y sin
comprimir) se pasará a un kernel compatible con Multiboot2 en las etiquetas *Módulos* (tipo 3).

El caso especial es si el módulo comienza con los bytes `DSDT`, `GUDT` o `0xD00DFEED`. En estos casos el archivo no se agregará al
*Módulos*, sino que se parcheará la tabla ACPI para que sus punteros DSDT apunten al contenido de este archivo. Con este
puede reemplazar fácilmente la tabla ACPI de una BIOS con errores por una proporcionada por el usuario.

### Logotipo de bienvenida de arranque

También puede mostrar un logotipo en el centro de la pantalla cuando se selecciona la opción de inicio si coloca una línea que
comienza con `bootsplash`. Debe estar precedido por una línea de `menuentry`.
```
bootsplash [#(fondo)] (ruta a un archivo tga)
```

El color de fondo es opcional y debe estar en notación HTML comenzando con una marca hash seguida de 6 dígitos hexadecimales, RRGGBB.
Si el primer argumento no comienza con `#`, entonces se supone un argumento de ruta.

La ruta debe apuntar a un archivo existente y debe ser una ruta UTF-8 absoluta en la partición de inicio (NO raíz). La imagen debe
estar en un formato Targa codificado y con mapas de colores, al igual que los íconos del menú. Las dimensiones pueden ser cualquier
cosa que quepa en la pantalla.

### Soporte multinúcleo

Para iniciar el kernel en todos los núcleos del procesador a la vez, especifique la directiva `multicore`. Debe estar precedido por
una línea de `menuentry` y también requiere que se instale el complemento [smp](../../src/plugins/smp.c).

```
multicore
```

Solución de problemas
---------------------

Si encuentra algún problema, simplemente ejecute con el indicador `easyboot -vv`. Esto realizará la validación y generará los
resultados detalladamente en el momento de la creación de la imagen. De lo contrario, agregue `verbose 3` a `easyboot/menu.cfg` para
obtener mensajes detallados sobre el tiempo de arranque.

Si ve la cadena `PMBR-ERR` en la esquina superior izquierda con un fondo rojo, significa que su CPU es muy antigua y no admite el
modo largo de 64 bits o que el sector de arranque no pudo arrancar el cargador. Puede ocurrir solo en máquinas con BIOS, esto nunca
puede suceder con UEFI o RaspberryPi.

| Mensaje                             | Descripción                                                                             |
|-------------------------------------|-----------------------------------------------------------------------------------------|
| `Booting [X]...`                    | indica que se eligió la opción de inicio (índice de menuentry) X                        |
| `Loading 'X' (Y bytes)...`          | se está cargando el archivo X de longitud Y                                             |
| `Parsing kernel...`                 | kernel fue encontrado, ahora detectando su formato                                      |
| `Starting X boot...`                | muestra que se detectó el gestor de arranque del sistema X                              |
| `Starting X kernel...`              | muestra que se detectó el núcleo del sistema X                                          |
| `Transfering X control to Y`        | indica que el punto de entrada del modo X en la dirección Y está a punto de ser llamado |

Si encuentra algún problema después de ver el último mensaje, significa que el problema ocurrió en el procedimiento de inicio del
sistema operativo y no en el cargador **Easyboot**, por lo que deberá consultar la documentación del sistema operativo indicado.
De lo contrario, no dude en abrir un [problema](https://gitlab.com/bztsrc/easyboot/-/issues) en gitlab.

### Multiboot1

Complementos necesarios: [grubmb1](../../src/plugins/grubmb1.c)

### Multiboot2

Este es el más flexible, con múltiples variaciones admitidas mediante complementos:

- ELF64 o PE32+ con Multiboot2 simplificado: no se requieren complementos
- ELF32 con Multiboot2 simplificado y punto de entrada de 32 bits: [elf32](../../src/plugins/elf32.c)
- a.out (struct exec) con Multiboot2 simplificado y punto de entrada de 32 bits: [aout](../../src/plugins/aout.c)
- Multiboot2 compatible con GRUB con punto de entrada de 32 bits: [grubmb2](../../src/plugins/grubmb2.c)

Tenga en cuenta la diferencia: el [Multiboot2 simplificado](ABI.md) no requiere etiquetas incrustadas, admite núcleos de mitad
superior, un punto de entrada limpio de 64 bits con parámetros pasados de acuerdo con las ABI de Multiboot2, SysV y fastcall también.

Por otro lado, [Multiboot2 compatible con GRUB](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) necesita
etiquetas integradas, no admite kernels de mitad superior ni de 64 bits, el punto de entrada siempre es de 32 bits y los parámetros
se pasan en `eax`, `ebx` solamente.

### Windows

No se requieren complementos, pero debe configurar [SecureBoot](secureboot.md).

```
menuentry win {
  kernel EFI/Microsoft/BOOT/BOOTMGRW.EFI
}
```

### Linux

Complementos necesarios: [linux](../../src/plugins/linux.c), [ext234](../../src/plugins/ext234.c)

Si el kernel no está ubicado en la partición de inicio, puede usar la línea de comando para especificar la partición raíz.

```
menuentry linux {
  kernel vmlinuz-linux root=PARTUUID=01020304-0506-0708-0a0b0c0d0e0f1011
}
```

### OpenBSD

Complementos necesarios: [obsdboot](../../src/plugins/obsdboot.c), [ufs44](../../src/plugins/ufs44.c)

```
menuentry openbsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot
}
```

¡ADVERTENCIA! ¡No utilice el complemento [elf32](../../src/plugins/elf32.c) si está iniciando OpenBSD! Su `boot` afirma
incorrectamente ser un ELF con un punto de entrada SysV ABI de 32 bits, pero en realidad tiene una entrada en modo real de 16 bits.

### FreeBSD

Complementos necesarios: [fbsdboot](../../src/plugins/fbsdboot.c), [ufs2](../../src/plugins/ufs2.c)

En sistemas BIOS heredados, especifique el cargador `boot`.

```
menuentry freebsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot/boot
}
```

En máquinas UEFI, utilice el cargador `loader.efi` en la partición de arranque (no se necesitan complementos).

```
menuentry freebsd {
  kernel boot/loader.efi
}
```

Si su sistema de archivos raíz es ZFS, copie este archivo (`boot` en BIOS, `loader.efi` en UEFI) a `(indir)` y NO especifique
una partición raíz.

### FreeDOS

Complementos necesarios: [fdos](../../src/plugins/fdos.c)

Mueva los archivos de FreeDOS a `(indir)` (FreeDOS usará la partición de inicio como partición raíz).

```
menuentry freedos {
  kernel KERNEL.SYS
}
```

Si el arranque se detiene después de imprimir los derechos de autor y `- InitDisk`, entonces el kernel de FreeDOS se compiló sin
soporte FAT32. Descargue un kernel diferente, con `f32` en su nombre.

### ReactOS

Complementos necesarios: [reactos](../../src/plugins/reactos.c)

```
menuentry reactos {
  kernel FREELDR.SYS
}
```

### MenuetOS

Complementos necesarios: [menuet](../../src/plugins/menuet.c)

```
menuentry menuetos {
  kernel KERNEL.MNT
  module CONFIG.MNT
  module RAMDISK.MNT
}
```

Para desactivar la configuración automática, agregue `noauto` a la línea de comando.

### KolibriOS

Complementos necesarios: [kolibri](../../src/plugins/kolibri.c)

```
menuentry kolibrios {
  kernel KERNEL.MNT syspath=/rd/1/ launcher_start=1
  module KOLIBRI.IMG
  module DEVICES.DAT
}
```

El complemento también funciona en máquinas UEFI, pero también puede usar `uefi4kos.efi` en la partición de inicio (y no se
requiere complemento).

### SerenityOS

Complementos necesarios: [grubmb1](../../src/plugins/grubmb1.c)

```
menuentry serenityos {
  kernel boot/Prekernel
  module boot/Kernel
}
```

### Haiku

Complementos necesarios: [grubmb1](../../src/plugins/grubmb1.c), [befs](../../src/plugins/befs.c)

```
menuentry haiku {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel system/packages/haiku_loader-r1~beta4_hrev56578_59-1-x86_64.hpkg
}
```

En máquinas UEFI, utilice el cargador `haiku_loader.efi` en la partición de arranque (no se necesitan complementos).
