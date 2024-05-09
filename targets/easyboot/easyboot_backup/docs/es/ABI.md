Escribiendo kernels compatibles con Easyboot
============================================

[Easyboot](https://gitlab.com/bztsrc/easyboot) admite varios núcleos usando [complementos](plugins.md). Pero si no se encuentra un
complemento adecuado, recurre a los binarios ELF64 o PE32+ con una variante simplificada (sin necesidad de incrustar nada) del
protocolo Multiboot2.

Este es el mismo protocolo que utiliza [Simpleboot](https://gitlab.com/bztsrc/simpleboot), todos los núcleos de ejemplo en ese
repositorio deben funcionar con **Easyboot** también.

Puede usar el encabezado multiboot2.h original en el repositorio de GRUB o el archivo de encabezado C/C++
[simpleboot.h](https://gitlab.com/bztsrc/simpleboot/blob/main/simpleboot.h) para que sea más fácil utilizar definiciones de tipo.
El formato binario de bajo nivel es el mismo, también puede usar cualquier biblioteca Multiboot2 existente, incluso con lenguajes
que no sean C, como este [Rust](https://github.com/rust-osdev/multiboot2/tree/main/ multiboot2/src), por ejemplo (nota: no estoy
afiliado a esos desarrolladores de ninguna manera, solo busqué "Rust Multiboot2" y ese fue el primer resultado).

[[_TOC_]]

Secuencia de inicio
-------------------

### Arrancando el cargador

En máquinas *BIOS*, el firmware carga el primer sector del disco en 0:0x7C00 y le pasa el control. En este sector **Easyboot** tiene
[boot_x86.asm](../../src/boot_x86.asm), que es lo suficientemente inteligente como para localizar y cargar el cargador de segunda
etapa, y también para configurar el modo largo.

En máquinas *UEFI*, el firmware carga directamente el mismo archivo de segunda etapa, llamado `EFI/BOOT/BOOTX64.EFI`. La fuente de
este cargador se puede encontrar en [loader_x86.c](../../src/loader_x86.c). Eso es todo, **Easyboot** no es GRUB ni syslinux, los
cuales requieren docenas y docenas de archivos del sistema en el disco. Aquí no se necesitan más archivos, solo este (los
complementos son opcionales, no se necesita ninguno para proporcionar compatibilidad con Multiboot2).

En *Raspberry Pi* el cargador se llama `KERNEL8.IMG`, compilado desde [loader_rpi.c](../../src/loader_rpi.c).

### El cargador

Este cargador está escrito con mucho cuidado para funcionar en múltiples configuraciones. Carga la tabla de particiones GUID desde
el disco y busca una "Partición del sistema EFI". Cuando lo encuentra, busca el archivo de configuración `easyboot/menu.cfg` en esa
partición de arranque. Una vez seleccionada la opción de arranque y conocido el nombre del archivo del kernel, el cargador lo
localiza y lo carga.

Luego detecta automáticamente el formato del kernel y es lo suficientemente inteligente como para interpretar la información de
sección y segmento sobre dónde cargar qué (realiza mapeo de memoria bajo demanda cuando es necesario). Luego configura un entorno
adecuado dependiendo del protocolo de arranque detectado (Multiboot2/Linux/etc. protegido o modo largo, argumentos ABI, etc.). Una
vez que el estado de la máquina es sólido y está bien definido, como último acto, el cargador salta al punto de entrada de su kernel.

Estado de la máquina
--------------------

Todo lo que está escrito en la [especificación Multiboot2](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) (en
inglés) sobre el estado de la máquina se mantiene, excepto los registros de propósito general. **Easyboot** pasa dos argumentos al
punto de entrada de su kernel de acuerdo con SysV ABI y también con Microsoft fastcall ABI. El primer parámetro es mágico, el
segundo es una dirección de memoria física, que apunta a una lista de etiquetas de información de arranque múltiple (abreviada como
MBI en lo sucesivo, ver más abajo).

También violamos un poco el protocolo Multiboot2 para manejar núcleos de mitad superior. Multiboot2 exige que la memoria debe tener
un mapeo de identidad. Bueno, bajo **Easyboot** esto es sólo parcialmente cierto: solo garantizamos que toda la RAM física seguramente
tenga la identidad asignada como se esperaba; sin embargo, es posible que algunas regiones superiores (dependiendo de los encabezados
del programa del kernel) aún estén disponibles. Esto no interrumpe los núcleos normales compatibles con Multiboot2, que se supone que
no deben acceder a la memoria fuera de la RAM física disponible.

Su kernel se carga exactamente de la misma manera en los sistemas BIOS y UEFI, así como en RPi, las diferencias de firmware son
simplemente "el problema de alguien más". Lo único que verá su kernel es si el MBI contiene la etiqueta de la tabla del sistema EFI
o no. Para simplificarle la vida, **Easyboot** tampoco genera la etiqueta del mapa de memoria EFI (tipo 17), solo proporciona la
etiqueta [Mapa de memoria](#mapa-de-memoria) (tipo 6) de forma indiscriminada en todas las plataformas (también en sistemas UEFI,
allí, el mapa de memoria simplemente se convierte para usted, por lo que su núcleo tiene que manejar sólo un tipo de etiqueta de
lista de memoria). Las etiquetas antiguas y obsoletas también se omiten y este administrador de arranque nunca las genera.

El kernel se ejecuta a nivel de supervisor (anillo 0 en x86, EL1 en ARM).

GDT no especificado, pero válido. La pila se configura en los primeros 640k y crece hacia abajo (pero debes cambiar esto lo antes
posible a cualquier pila que consideres valiosa).

Debería considerar IDT como no especificado; IRQ, NMI e interrupciones de software deshabilitadas. Los controladores de excepciones
ficticios están configurados para mostrar un volcado mínimo y detener la máquina. Sólo se debe confiar en estos para informar si su
kernel sufre estragos antes de que pudiera configurar su propio IDT y sus controladores, preferiblemente lo antes posible. En ARM,
vbar_el1 está configurado para llamar a los mismos controladores de excepciones ficticios (aunque, por supuesto, volcan registros
diferentes).

Framebuffer también está configurado de forma predeterminada. Puede modificar la resolución en la configuración, pero si no se
proporciona, el framebuffer aún está configurado.

Es importante no regresar nunca de su kernel. Eres libre de sobrescribir cualquier parte del cargador en la memoria (tan pronto como
hayas terminado con las etiquetas MBI), por lo que simplemente no hay ningún lugar al que regresar. "Der Mohr hat seine Schuldigkeit
getan, der Mohr kann gehen".

Información de arranque pasada a su kernel (MBI)
------------------------------------------------

No es obvio al principio, pero la especificación Multiboot2 en realidad define dos conjuntos de etiquetas totalmente independientes:

- Se supone que el primer conjunto está incluido en un kernel compatible con Multiboot2, llamado encabezado Multiboot2 de la imagen
  del sistema operativo (sección 3.1.2), por lo tanto *proporcionado por el kernel*. A **Easyboot** no le importan estas etiquetas y
  tampoco las analiza en su kernel. Simplemente no necesita ningún dato mágico especial incrustado en su archivo de kernel;
  **Easyboot**, los encabezados ELF y PE son suficientes.

- El segundo conjunto se *pasa al kernel* dinámicamente al arrancar, **Easyboot** usa solo estas etiquetas. Sin embargo, no genera
  todo lo que especifica Multiboot2 (simplemente omite los antiguos, obsoletos o heredados). Estas etiquetas se denominan etiquetas
  MBI; consulte [Información de arranque](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Boot-information-format)
  (sección 3.6).

NOTA: la especificación Multiboot2 en las etiquetas MBI tiene muchos errores. Puede encontrar una versión corregida a continuación,
que se alinea con el archivo de encabezado multiboot2.h que puede encontrar en el repositorio fuente de GRUB.

El primer parámetro de su kernel es el mágico 0x36d76289 (en `rax`, `rcx` y `rdi`). Puede ubicar las etiquetas MBI usando el segundo
parámetro (en `rbx`, `rdx` y `rsi`). En la plataforma ARM, la magia está en `x0` y la dirección está en `x1`. En RISC-V y MIPS se
utilizan `a0` y `a1`, respectivamente. Si este cargador se traslada a otra arquitectura, siempre se deben utilizar los registros
especificados por SysV ABI para los argumentos de la función. Si hay otras ABI comunes en la plataforma que no interfieren con la
ABI de SysV, entonces los valores también deben duplicarse en los registros de esas ABI (o en la parte superior de la pila).

### Encabezados

La dirección pasada siempre está alineada con 8 bytes y comienza con un encabezado MBI:

```
        +-------------------+
u32     | total_size        |
u32     | reserved          |
        +-------------------+
```

A esto le sigue una serie de etiquetas alineadas también de 8 bytes. Cada etiqueta comienza con los siguientes campos de encabezado
de etiqueta:

```
        +-------------------+
u32     | type              |
u32     | size              |
        +-------------------+
```

`type` contiene un identificador del contenido del resto de la etiqueta. `size` contiene el tamaño de la etiqueta, incluidos los
campos de encabezado pero sin incluir el relleno. Las etiquetas se suceden unas a otras y se rellenan cuando es necesario para que
cada etiqueta comience en una dirección alineada de 8 bytes.

### Terminador

```
        +-------------------+
u32     | type = 0          |
u32     | size = 8          |
        +-------------------+
```

Las etiquetas terminan en una etiqueta de tipo `0` y tamaño `8`.

### Línea de comando de arranque

```
        +-------------------+
u32     | type = 1          |
u32     | size              |
u8[n]   | string            |
        +-------------------+
```

`string` contiene la línea de comando especificada en la línea `kernel` de *menuentry* (sin la ruta del kernel ni el nombre de
archivo). La línea de comando es una cadena UTF-8 normal de estilo C terminada en cero.

### Nombre del cargador de arranque

```
        +----------------------+
u32     | type = 2             |
u32     | size = 17            |
u8[n]   | string "Easyboot"    |
        +----------------------+
```

`string` contiene el nombre de un cargador de arranque que arranca el kernel. El nombre es una cadena terminada en cero UTF-8 de
estilo C normal.

### Módulos

```
        +-------------------+
u32     | type = 3          |
u32     | size              |
u32     | mod_start         |
u32     | mod_end           |
u8[n]   | string            |
        +-------------------+
```

Esta etiqueta indica al kernel qué módulo de arranque se cargó junto con la imagen del kernel y dónde se puede encontrar. `mod_start`
y `mod_end` contienen las direcciones físicas de inicio y fin del propio módulo de arranque. Nunca obtendrá un búfer comprimido con
gzip, porque **Easyboot** los descomprime de forma transparente (y si proporciona un complemento, también funciona con datos que no
sean comprimidos con gzip). El campo `cadena` proporciona una cadena arbitraria que se asociará con ese módulo de inicio en
particular; es una cadena UTF-8 normal de estilo C terminada en cero. Se especifica en la línea `module` de *menuentry* y su uso
exacto es específico del sistema operativo. A diferencia de la etiqueta de la línea de comando de arranque, las etiquetas del módulo
*también incluyen* la ruta y el nombre del archivo del módulo.

Aparece una etiqueta por módulo. Este tipo de etiqueta puede aparecer varias veces. Si se cargó un disco ram inicial junto con su
kernel, aparecerá como el primer módulo.

Hay un caso especial: si el archivo es una tabla DSDT ACPI, un blob FDT (dtb) o GUDT, entonces no aparecerá como un módulo, sino
que se parcheará el antiguo RSDP ACPI (tipo 14) o el nuevo RSDP ACPI (tipo 15). y su DSDT se reemplaza con el contenido de este
archivo.

### Mapa de memoria

Esta etiqueta proporciona un mapa de memoria.

```
        +-------------------+
u32     | type = 6          |
u32     | size              |
u32     | entry_size = 24   |
u32     | entry_version = 0 |
varies  | entries           |
        +-------------------+
```

`size` contiene el tamaño de todas las entradas, incluido este campo. `entry_size` siempre es 24. `entry_version` se establece en `0`.
Cada entrada tiene la siguiente estructura:

```
        +-------------------+
u64     | base_addr         |
u64     | length            |
u32     | type              |
u32     | reserved          |
        +-------------------+
```

`base_addr` es la dirección física inicial. `longitud` es el tamaño de la región de memoria en bytes. "tipo" es la variedad del
rango de direcciones representado, donde un valor de `1` indica RAM disponible, el valor de `3` indica memoria utilizable que
contiene información ACPI, el valor de `4` indica memoria reservada que debe conservarse en hibernación, El valor de `5` indica una
memoria que está ocupada por módulos RAM defectuosos y todos los demás valores indican actualmente un área reservada. `reserved` se
establece en `0` en el arranque del BIOS.

Cuando el MBI se genera en una máquina UEFI, varias entradas del mapa de memoria EFI se almacenan como tipo `1` (RAM disponible) o
`2` (RAM reservada) y, si lo necesita, el tipo de memoria EFI original se coloca en el campo `reserved`.

Se garantiza que el mapa proporcionado enumera toda la RAM estándar que debería estar disponible para un uso normal, y siempre está
ordenado en orden ascendente `base_addr`. Sin embargo, este tipo de RAM disponible incluye las regiones ocupadas por kernel, mbi,
segmentos y módulos. El kernel debe tener cuidado de no sobrescribir estas regiones (**Easyboot** podría excluir fácilmente esas
regiones, pero eso rompería la compatibilidad con Multiboot2).

### Información del framebuffer

```
        +----------------------------------+
u32     | type = 8                         |
u32     | size = 38                        |
u64     | framebuffer_addr                 |
u32     | framebuffer_pitch                |
u32     | framebuffer_width                |
u32     | framebuffer_height               |
u8      | framebuffer_bpp                  |
u8      | framebuffer_type = 1             |
u16     | reserved                         |
u8      | framebuffer_red_field_position   |
u8      | framebuffer_red_mask_size        |
u8      | framebuffer_green_field_position |
u8      | framebuffer_green_mask_size      |
u8      | framebuffer_blue_field_position  |
u8      | framebuffer_blue_mask_size       |
        +----------------------------------+
```

El campo `framebuffer_addr` contiene la dirección física del framebuffer. El campo `framebuffer_pitch` contiene la longitud de una
fila en bytes. Los campos `framebuffer_width`, `framebuffer_height` contienen dimensiones del framebuffer en píxeles. El campo
`framebuffer_bpp` contiene el número de bits por píxel. `framebuffer_type` siempre se establece en 1 y `reserved` siempre contiene 0
en la versión actual de la especificación y la imagen del sistema operativo debe ignorarlo. Los campos restantes describen el
formato de píxeles empaquetados, la posición de los canales y el tamaño en bits. Puede usar la expresión
`((~(0xffffffff << size)) << position) & 0xffffffff` para obtener una máscara de canal similar a UEFI GOP.

### Puntero de tabla del sistema EFI de 64 bits

Esta etiqueta solo existe si **Easyboot** se ejecuta en una máquina UEFI. En una máquina con BIOS, esta etiqueta nunca se generó.

```
        +-------------------+
u32     | type = 12         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

Esta etiqueta contiene un puntero a la tabla del sistema EFI.

### Puntero de identificador de imagen EFI de 64 bits

Esta etiqueta solo existe si **Easyboot** se ejecuta en una máquina UEFI. En una máquina con BIOS, esta etiqueta nunca se generó.

```
        +-------------------+
u32     | type = 20         |
u32     | size = 16         |
u64     | pointer           |
        +-------------------+
```

Esta etiqueta contiene un puntero al identificador de imagen EFI. Por lo general, es el identificador de imagen del cargador de
arranque.

### SMBIOS tables

```
        +-------------------+
u32     | type = 13         |
u32     | size              |
u8      | major             |
u8      | minor             |
u8[6]   | reserved          |
        | smbios tables     |
        +-------------------+
```

Esta etiqueta contiene una copia de las tablas SMBIOS así como su versión.

### ACPI antiguo RSDP

```
        +-------------------+
u32     | type = 14         |
u32     | size              |
        | copy of RSDPv1    |
        +-------------------+
```

Esta etiqueta contiene una copia de RSDP según lo definido por la especificación ACPI 1.0. (Con una dirección de 32 bits).

### ACPI nuevo RSDP

```
        +-------------------+
u32     | type = 15         |
u32     | size              |
        | copy of RSDPv2    |
        +-------------------+
```

Esta etiqueta contiene una copia de RSDP según lo definido por ACPI 2.0 o especificación posterior. (Probablemente con una dirección
de 64 bits).

Estos (tipo 14 y 15) apuntan a una tabla `RSDT` o `XSDT` con un puntero a una tabla `FACP`, que a su vez contiene dos punteros a una
tabla `DSDT`, que describe la máquina. **Easyboot** falsifica estas tablas en máquinas que de otro modo no son compatibles con ACPI.
Además, si proporciona una tabla DSDT, un blob FDT (dtb) o GUDT como módulo, **Easyboot** parcheará los punteros para que apunten a
esa tabla proporcionada por el usuario. Para analizar estas tablas, puede usar mi biblioteca de encabezado único y libre de
dependencias [hwdet](https://gitlab.com/bztsrc/hwdet) (o la inflada [apcica](https://github.com/acpica/acpica) y
[libfdt](https://github.com/dgibson/dtc)).

Etiquetas específicas del kernel
--------------------------------

Las etiquetas con `type` mayor o igual a 256 no forman parte de la especificación Multiboot2. Estos pueden agregarse mediante
[complementos](plugins.md) opcionales a la lista, si un kernel los necesita.

### EDID

```
        +-------------------+
u32     | type = 256        |
u32     | size              |
        | copy of EDID      |
        +-------------------+
```

Esta etiqueta contiene una copia de la lista de resoluciones de monitores compatibles según la especificación EDID.

### SMP

```
        +-------------------+
u32     | type = 257        |
u32     | size              |
u32     | numcores          |
u32     | running           |
u32     | bspid             |
        +-------------------+
```

Esta etiqueta existe si se proporcionó la directiva `multicore`. `numcores` contiene la cantidad de núcleos de CPU en el sistema,
`running` es la cantidad de núcleos que se han inicializado exitosamente y ejecutan el mismo kernel en paralelo. El `bspid` contiene
el identificador del núcleo BSP (en la identificación lAPIC x86), de modo que los núcleos puedan distinguir los AP y ejecutar un
código diferente en ellos. Todos los AP tienen su propia pila y encima de la pila estará la identificación del núcleo actual.

Diseño de memoria
-----------------

### Máquinas BIOS

| Inicio   | Fin     | Descripción                                                                       |
|---------:|--------:|-----------------------------------------------------------------------------------|
|      0x0 |   0x400 | Interrupt Vector Table (utilizable, IDT en modo real)                             |
|    0x400 |   0x4FF | BIOS Data Area (utilizable)                                                       |
|    0x4FF |   0x500 | Código de la unidad de arranque del BIOS (probablemente 0x80, utilizable)         |
|    0x500 |  0x1000 | pila de controlador de excepciones (utilizable después de configurar su IDT)      |
|   0x1000 |  0x8000 | tablas de paginación (utilizable después de configurar las tablas de paginación)  |
|   0x8000 | 0x20000 | código y datos del cargador (utilizable después de configurar su IDT)             |
|  0x20000 | 0x40000 | configuración + etiquetas (utilizable después de analizar MBI)                    |
|  0x40000 | 0x90000 | plugin ids; de arriba a abajo: pila del kernel                                    |
|  0x90000 | 0x9A000 | Solo kernel de Linux: zero page + cmdline                                         |
|  0x9A000 | 0xA0000 | Extended BIOS Data Area (mejor no tocar)                                          |
|  0xA0000 | 0xFFFFF | VRAM y BIOS ROM (no utilizable)                                                   |
| 0x100000 |       x | segmentos del kernel, seguidos de los módulos, cada página alineada               |

### Máquinas UEFI

Nadie lo sabe. UEFI asigna memoria como quiere. Espere cualquier cosa, pero lo más probable es que se ubique por debajo de 64
millones. Seguramente todas las áreas aparecerán en el mapa de memoria como tipo = 1 (`MULTIBOOT_MEMORY_AVAILABLE`) y reserved = 2
(`EfiLoaderData`), sin embargo, esto no es exclusivo, otros tipos de memoria también podrían aparecer así (bss del administrador de
arranque). sección, por ejemplo).

### Raspberry Pi

| Inicio   | Fin     | Descripción                                                                       |
|---------:|--------:|-----------------------------------------------------------------------------------|
|      0x0 |   0x100 | reservado por firmware (mejor no tocar)                                           |
|    0x100 |  0x1000 | pila de controlador de excepciones (utilizable después de configurar su VBAR)     |
|   0x1000 |  0x9000 | tablas de paginación (utilizable después de configurar las tablas de paginación)  |
|   0x9000 | 0x20000 | código y datos del cargador (utilizable después de configurar su VBAR)            |
|  0x20000 | 0x40000 | configuración + etiquetas (utilizable después de analizar MBI)                    |
|  0x40000 | 0x80000 | firmware proporcionado FDT (dtb); de arriba a abajo: pila del kernel              |
| 0x100000 |       x | segmentos del kernel, seguidos de los módulos, cada página alineada               |

Los primeros bytes están reservados para [armstub](https://github.com/raspberrypi/tools/blob/master/armstubs/armstub8.S). Solo se
inició el núcleo 0, por lo que para iniciar los procesadores de aplicaciones, escriba la dirección de una función en 0xE0 (núcleo 1),
0xE8 (núcleo 2), 0xF0 (núcleo 3), cuyas direcciones se encuentran en esta área.

Aunque no es compatible de forma nativa con RPi, aún obtienes una etiqueta RSDP (tipo 14) antigua de ACPI, con tablas falsas. La
tabla `APIC` se utiliza para comunicar la cantidad de núcleos de CPU disponibles al kernel. La dirección de la función de inicio se
almacena en el campo RSD PTR -> RSDT -> APIC -> cpu\[x].apic_id (y el ID del núcleo en cpu\[x].acpi_id, donde BSP siempre es
cpu\[0].acpi_id = 0 y cpu\[0].apic_id = 0xD8. Cuidado, "acpi" y "apic" se ven bastante similares).

Si el firmware pasa un blob FDT válido, o si uno de los módulos es un archivo .dtb, .gud o .aml, también se agrega una tabla FADT
(con `FACP` mágica). En esta tabla, el puntero DSDT (32 bits, con desplazamiento 40) apunta al blob de árbol de dispositivos
aplanado proporcionado.

Aunque el firmware no proporciona una función de mapa de memoria, también recibirá una etiqueta de mapa de memoria (tipo 6), que
enumera la RAM detectada y la región MMIO. Puede usar esto para detectar la dirección base del MMIO, que es diferente en RPi3 y RPi4.
