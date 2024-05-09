Complementos de Easyboot
========================

De forma predeterminada, [Easyboot](https://gitlab.com/bztsrc/easyboot) arranca núcleos compatibles con Multiboot2 en formatos ELF y
PE desde la partición de arranque. Si su kernel usa un formato de archivo diferente, un protocolo de inicio diferente o no está
ubicado en la partición de inicio, necesitará complementos en la partición de inicio. Puede encontrarlos en el directorio
[src/plugins](../../src/plugins).

[[_TOC_]]

Instalación
-----------

Para instalar complementos, simplemente cópielos en el directorio especificado en el parámetro `(indir)`, en el subdirectorio
`easyboot` al lado del archivo menu.cfg.

Por ejemplo:
```
bootpart
|-- easyboot
|   |-- linux_x86.plg
|   |-- minix_x86.plg
|   `-- menu.cfg
|-- EFI
|   `-- BOOT
|-- kernel
`-- initrd
$ easyboot bootpart disco.img
```

Compilacion
-----------

*Era obvio desde el principio que ELF no es adecuado para esta tarea. Es demasiado inflado y demasiado complejo. Originalmente
quería usar struct exec (el formato clásico a.out de UNIX), pero desafortunadamente las cadenas de herramientas modernas ya no
pueden crear eso. Así que he decidido crear mi propio formato y mi propio enlazador para los complementos.*

Puede compilar el código fuente del complemento con cualquier compilador cruzado ANSI C estándar en un archivo objeto ELF, pero
luego tendrá que usar el vinculador [plgld](../../src/misc/plgld.c) para crear el binario final. Este es un entrelazador
independiente de la arquitectura y funcionará sin importar para qué código de máquina se compiló el complemento. El .plg final
tiene solo una fracción del tamaño del .o ELF a partir del cual se generó.

### API de complemento

El código fuente C de un complemento debe incluir el archivo de encabezado `src/loader.h` y debe contener una línea `EASY_PLUGIN`.
Tiene un parámetro, el tipo de complemento, seguido de la especificación de coincidencia del identificador. El cargador utiliza
este último para determinar cuándo usar ese complemento en particular.

Por ejemplo:

```c
#include "../loader.h"

/* bytes mágicos que identifican un kernel de Linux */
EASY_PLUGIN(PLG_T_KERNEL) {
   /* compensar  talla  tipo de partido  bytes mágicos */
    { 0x1fe,        2,  PLG_M_CONST,   { 0xAA, 0x55, 0, 0 } },
    { 0x202,        4,  PLG_M_CONST,   { 'H', 'd', 'r', 'S' } }
};

/* punto de entrada, el prototipo está definido por el tipo de complemento */
PLG_API void _start(uint8_t *buf, uint64_t size);
{
    /* preparar el entorno para un kernel de Linux */
}
```

Los complementos pueden usar varias variables y funciones, todas ellas definidas en el archivo de encabezado y vinculadas en tiempo
de ejecución.

```c
uint32_t verbose;
```
Nivel de verbosidad. Un complemento solo puede imprimir cualquier salida si es distinta de cero, excepto mensajes de error. Cuanto
mayor sea su valor, más detalles se deberán imprimir.

```c
uint64_t file_size;
```
El tamaño total del archivo abierto (consulte `open` y `loadfile` a continuación).

```c
uint8_t *root_buf;
```
Cuando se inicializa un complemento del sistema de archivos, contiene los primeros 128k de la partición (con suerte, incluido el
superbloque). Más tarde, un complemento del sistema de archivos puede reutilizar este búfer de 128k para cualquier propósito (caché
FAT, caché de inodo, etc.)

```c
uint8_t *tags_buf;
```
Contiene las etiquetas Multiboot2. Un complemento del kernel puede analizar esto para convertir los datos proporcionados por el
administrador de arranque al formato que espera el kernel. Este puntero apunta al inicio del búfer.

```c
uint8_t *tags_ptr;
```
Este puntero apunta al final del búfer de etiquetas Multiboot2. Los complementos de etiquetas pueden agregar nuevas etiquetas aquí
y ajustar este puntero.

```c
uint8_t *rsdp_ptr;
```
Apunta al puntero ACPI de RSDP.

```c
uint8_t *dsdt_ptr;
```
Apunta al blob de descripción de hardware DSDT (o GUDT, FDT).

```c
efi_system_table_t *ST;
```
En máquinas UEFI, apunta a la tabla del sistema EFI; de lo contrario, `NULL`.

```c
void memset(void *dst, uint8_t c, uint32_t n);
void memcpy(void *dst, const void *src, uint32_t n);
int  memcmp(const void *s1, const void *s2, uint32_t n);
```
Funciones de memoria obligatorias (el compilador de C puede generar llamadas a estas, incluso cuando no hay una llamada directa).

```c
void *alloc(uint32_t num);
```
Asigna `num` páginas (4k) de memoria. Los complementos no deben asignar mucho, deben apuntar a un uso mínimo de memoria.

```c
void free(void *buf, uint32_t num);
```
Libere memoria previamente asignada de páginas `num`.

```c
void printf(char *fmt, ...);
```
Imprima una cadena formateada en la consola de inicio.

```c
uint64_t pb_init(uint64_t size);
```
Inicia la barra de progreso, `size` es el tamaño total que representa. Devuelve cuántos bytes vale un píxel.

```c
void pb_draw(uint64_t curr);
```
Dibuja la barra de progreso para el valor actual. `curr` debe estar entre 0 y el tamaño total.

```c
void pb_fini(void);
```
Cierra la barra de programas, borra su lugar en la pantalla.

```c
void loadsec(uint64_t sec, void *dst);
```
Utilizado por los complementos del sistema de archivos, carga un sector del disco en la memoria. `sec` es el número del sector,
relativo a la partición raíz.

```c
void sethooks(void *o, void *r, void *c);
```
Utilizado por los complementos del sistema de archivos, establece los enlaces de las funciones de open / read / close para el
sistema de archivos de la partición raíz.

```c
int open(char *fn);
```
Abra un archivo en la partición raíz (o de inicio) para leerlo; devuelve 1 en caso de éxito. Sólo se puede abrir un archivo a la vez.
Cuando no hubo una llamada `sethooks` de antemano, opera en la partición de inicio.

```c
uint64_t read(uint64_t offs, uint64_t size, void *buf);
```
Lee datos del archivo abierto en la posición de búsqueda `offs` en la memoria y devuelve el número de bytes realmente leídos.

```c
void close(void);
```
Cierra el archivo abierto.

```c
uint8_t *loadfile(char *path);
```
Cargue un archivo completamente desde la partición raíz (o de inicio) en un búfer de memoria recién asignado y descomprímalo de
forma transparente si se encuentra el complemento. Tamaño devuelto en `file_size`.

```c
int loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz);
```
Cargue un segmento del búfer del kernel. Esto verifica si la memoria `vaddr` está disponible y asigna el segmento si es la mitad
superior. Los `offs` son el desplazamiento del archivo, por lo que son relativos al búfer del kernel. Si `memsz` es mayor que
`filesz`, entonces la diferencia se completa con ceros.

```c
void _start(void);
```
Punto de entrada para complementos del sistema de archivos (`PLG_T_FS`). Debería analizar el superbloque en `root_buf` y llamar a
`sethooks`. En caso de error, debería regresar sin fijar sus ganchos.

```c
void _start(uint8_t *buf, uint64_t size);
```
Punto de entrada para complementos del kernel (`PLG_T_KERNEL`). Recibe la imagen del kernel en la memoria, debe reubicar sus
segmentos, configurar el entorno adecuado y transferir el control. Cuando no hay ningún error, nunca regresa.

```c
uint8_t *_start(uint8_t *buf);
```
Punto de entrada para complementos de descompresor (`PLG_T_DECOMP`). Recibe el búfer comprimido (y su tamaño en `file_size`) y debe
devolver un nuevo búfer asignado con los datos sin comprimir (y establecer el nuevo tamaño del búfer también en `file_size`). Debe
liberar el búfer antiguo (cuidado, `file_size` está en bytes, pero free() espera el tamaño en páginas). En caso de error,
`file_size` no se debe cambiar y debe devolver el búfer original sin modificar.

```c
void _start(void);
```
Punto de entrada para complementos de etiquetas (`PLG_T_TAG`). Podrían agregar nuevas etiquetas en `tags_ptr` y ajustar ese puntero
a una nueva posición alineada de 8 bytes.

### Funciones locales

Los complementos pueden usar funciones locales; sin embargo, debido a un error de CLang, *deben* declararse como `static`. (El
error es que CLang genera registros PLT para esas funciones, aunque se pasa el indicador "-fno-plt" en la línea de comando. El uso
de `static` soluciona esto).

Especificación de formato de archivo de bajo nivel
--------------------------------------------------

En caso de que alguien quiera escribir un complemento en un lenguaje que no sea C (en Assembly, por ejemplo), aquí está la
descripción de bajo nivel del formato de archivo.

Es muy similar al formato a.out. El archivo consta de un encabezado de tamaño fijo, seguido de secciones de longitud variable. No
hay encabezado de sección, los datos de cada sección siguen directamente a los datos de la sección anterior en el siguiente orden:

```
(encabezamiento)
(registros de coincidencia de identificadores)
(registros de reubicación)
(codigo de maquina)
(datos de solo lectura)
(datos legibles y grabables inicializados)
```

Para la primera sección real, código máquina, se incluye la alineación. Para todas las demás secciones, se agrega relleno al tamaño
de la sección anterior.

SUGERENCIA: si pasa un complemento como argumento único a `plgld`, volcará las secciones del archivo con una salida similar a
`readelf -a` u `objdump -xd`.

### Encabezamiento

Todos los números están en formato little-endian independientemente de la arquitectura.

| Inicio  | Talle | Descripción                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     4 | bytes mágicos `EPLG`                                     |
|       4 |     4 | tamaño total del archivo                                 |
|       8 |     4 | memoria total requerida cuando se carga el archivo       |
|      12 |     4 | tamaño de la sección de código                           |
|      16 |     4 | tamaño de la sección de datos de solo lectura            |
|      20 |     4 | punto de entrada del complemento                         |
|      24 |     2 | Código de arquitectura (igual que los ELF)               |
|      26 |     2 | número de registros de reubicación                       |
|      28 |     1 | número de registros de coincidencia de identificadores   |
|      29 |     1 | entrada GOT con mayor referencia                         |
|      30 |     1 | revisión del formato de archivo (0 por ahora)            |
|      31 |     1 | tipo de complemento (1=sistema de archivos, 2=kernel, 3=descompresor, 4=tag) |

El código de arquitectura es el mismo que en los encabezados ELF, por ejemplo 62 = x86_64, 183 = Aarch64 y 243 = RISC-V.

El tipo de complemento especifica el prototipo del punto de entrada, la ABI siempre es SysV.

### Sección de coincidencia de identificadores

Esta sección contiene tantos de los siguientes registros como se especifican en el campo "número de registros de coincidencia de
identificadores" del encabezado.

| Inicio  | Talle | Descripción                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     2 | compensar                                                |
|       2 |     1 | talle                                                    |
|       3 |     1 | tipo                                                     |
|       4 |     4 | bytes mágicos para igualar                               |

Primero, el comienzo del tema se carga en un búfer. Se configura un acumulador, inicialmente con un 0. Las compensaciones en estos
registros siempre son relativas a este acumulador y abordan ese byte en el búfer.

El campo Tipo indica cómo interpretar el desplazamiento. Si es 1, entonces el desplazamiento más el acumulador se utiliza como valor.
Si es 2, entonces se toma un valor de byte de 8 bits en el desplazamiento, 3 significa tomar un valor de palabra de 16 bits y 4
significa tomar un valor de palabra d de 32 bits. 5 significa tomar un valor de byte de 8 bits y agregarle el acumulador, 6 significa
tomar un valor de palabra de 16 bits y agregarle el acumulador, y 7 es lo mismo pero con un valor de 32 bits. 8 buscará los bytes
mágicos desde el byte del acumulador hasta el final del búfer en pasos de desplazamiento y, si los encuentra, devuelve el
desplazamiento coincidente como valor.

Si el tamaño es cero, entonces el acumulador se establece en el valor. Si el tamaño no es cero, entonces se verifica esa cantidad de
bytes si coinciden con los bytes mágicos dados.

Por ejemplo, para comprobar si un ejecutable PE comienza con una instrucción NOP:
```c
EASY_PLUGIN(PLG_T_KERNEL) {
   /* compensar  talla  tipo de partido  bytes mágicos */
    { 0,            2,  PLG_M_CONST,  { 'M', 'Z', 0, 0 } }, /* comprobar bytes mágicos */
    { 60,           0,  PLG_M_DWORD,  { 0, 0, 0, 0 } },     /* obtener el desplazamiento del encabezado PE al acumulador */
    { 0,            4,  PLG_M_CONST,  { 'P', 'E', 0, 0 } }, /* comprobar bytes mágicos */
    { 40,           1,  PLG_M_DWORD,  { 0x90, 0, 0, 0 } }   /* verifique la instrucción NOP en el punto de entrada */
};
```

### Sección de reubicación

Esta sección contiene tantos de los siguientes registros como se especifican en el campo "número de registros de reubicación" del
encabezado.

| Inicio  | Talle | Descripción                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     4 | inicio                                                   |
|       4 |     4 | tipo de reubicación                                      |

Significado de los bits en tipo:

| Desde   | Hasta | Descripción                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     7 | símbolo (0 - 255)                                        |
|       8 |     8 | direccionamiento relativo de PC                          |
|       9 |     9 | GOT direccionamiento indirecto relativo                  |
|      10 |    13 | índice de máscara inmediata (0 - 15)                     |
|      14 |    19 | bit de inicio (0 - 63)                                   |
|      20 |    25 | bit final (0 - 63)                                       |
|      26 |    31 | posición del indicador bit de dirección negada (0 - 63)  |

El campo de desplazamiento es relativo a la magia en el encabezado del complemento y selecciona un número entero en la memoria donde
se debe realizar la reubicación.

El símbolo indica qué dirección utilizar. 0 significa la dirección BASE donde se cargó el complemento en la memoria, también
conocido como. la dirección de la magia del encabezado en la memoria. Otros valores seleccionan una dirección de símbolo externo del
GOT, definido en el cargador o en otro complemento, eche un vistazo a la matriz `plg_got` en la fuente de plgld.c para ver qué valor
corresponde a qué símbolo. Si el bit relativo GOT es 1, entonces se utiliza la dirección de la entrada GOT del símbolo, en lugar de
la dirección real del símbolo.

Si el bit relativo de la PC es 1, entonces el desplazamiento se resta primero de la dirección (modo de direccionamiento relativo del
puntero de intrusión).

El índice de máscara inmediata indica qué bits almacenan la dirección en la instrucción. Si es 0, entonces la dirección se escribe
tal cual en el desplazamiento, independientemente de la arquitectura. Para x86_64, solo se permite el índice 0. Para ARM Aarch64:
0 = tal cual, 1 = 0x07ffffe0 (desplazamiento a la izquierda 5 bits), 2 = 0x07fffc00 (desplazamiento a la izquierda 10 bits),
3 = 0x60ffffe0 (con instrucciones ADR/ADRP, el inmediato se desplaza y se divide en dos grupos de bits). Las arquitecturas futuras
podrían definir más y diferentes máscaras de bits inmediatas.

Usando la máscara inmediata, se toman de la memoria los bits final - inicio + 1 y se firman como extendidos. Este valor se agrega a
la dirección (se suma, y en el caso de referencias internas, la dirección del símbolo interno también se codifica aquí).

Si el bit indicador de dirección negada no es 0 y la dirección es positiva, entonces ese bit se borra. Si la dirección es negativa,
entonces se establece ese bit y se niega la dirección.

Finalmente, los bits de inicio y fin seleccionan qué parte de la dirección escribir en el entero seleccionado. Esto también define
el tamaño de la reubicación, los bits fuera de este rango y los que no forman parte de la máscara inmediata se dejan sin cambios.

### Sección de código

Esta sección contiene instrucciones de máquina para la arquitectura especificada en el encabezado y tiene tantos bytes como indica
el campo de tamaño del código.

### Sección de datos de solo lectura

Esta es una sección opcional, puede que falte. Es tan largo como lo indica el campo de tamaño de sección de solo lectura en el
encabezado. Todas las variables constantes se colocan en esta sección.

### Sección de datos inicializados

Esta es una sección opcional, puede que falte. Si todavía hay bytes en el archivo después de la sección de código (o la sección de
datos opcional de solo lectura), entonces todos esos bytes se consideran la sección de datos. Si una variable se inicializa con un
valor distinto de cero, se coloca en esta sección.

### Sección BSS

Esta es una sección opcional, puede que falte. Esta sección nunca se almacena en el archivo. Si el campo de tamaño en la memoria es
mayor que el campo de tamaño del archivo en el encabezado, entonces su diferencia se completará con ceros en la memoria. Si una
variable no se inicializa o se inicializa como cero, se coloca en esta sección.
