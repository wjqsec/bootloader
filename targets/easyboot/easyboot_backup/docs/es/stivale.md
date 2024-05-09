Soporte de stivale en Easyboot
==============================

No va a suceder, nunca jamás. Este protocolo de arranque tiene algunas malas decisiones de diseño muy graves y supone un enorme
riesgo para la seguridad.

Primero, los núcleos de stivale tienen un encabezado ELF, pero de alguna manera se supone que debes saber que el encabezado no es
válido; nada, absolutamente nada implica en el encabezado que no sea un kernel SysV ABI. En Multiboot debe haber algunos bytes
mágicos al principio del archivo para que puedas detectar el protocolo; no hay nada de eso en stivale/stivale2. (El ancla no le
ayuda, porque eso puede ocurrir *en cualquier lugar* del archivo, por lo que en realidad debe *buscar en todo el archivo* para
asegurarse de que no sea compatible con stivale).

Segundo, utiliza secciones; que de acuerdo con la especificación ELF (ver [página 46](https://www.sco.com/developers/devspecs/gabi41.pdf))
son opcionales y ningún cargador debería preocuparse por ello. Los cargadores usan la Vista de ejecución y no la Vista de enlace.
Implementar el análisis de secciones solo por este protocolo es una sobrecarga increíble en cualquier cargador, donde los recursos
del sistema generalmente ya son escasos.

En tercer lugar, los encabezados de las secciones se encuentran al final del archivo. Esto significa que para detectar stivale, debe
cargar el comienzo del archivo, analizar los encabezados ELF, luego cargar el final del archivo y analizar los encabezados de las
secciones, y luego cargar en algún lugar desde el medio del archivo para obtener la lista de etiquetas. Esta es la peor solución
posible que podría haber. Y nuevamente, no hay absolutamente nada que indique que un cargador deba hacer esto, por lo que debe
hacerlo para todos los kernels solo para descubrir que el kernel no usa stivale. Esto también ralentiza la detección de *todos los
demás* protocolos de arranque, lo cual es inaceptable.

Los procesadores de aplicaciones sondean activamente la lista de etiquetas, y el kernel puede llamar al código del cargador de
arranque en cualquier momento, lo que significa que simplemente no puede recuperar la memoria del cargador de arranque, de lo
contrario se garantiza una falla. Esto va en contra de la filosofía de **Easyboot**.

La peor parte es que el protocolo espera que los cargadores de arranque inyecten código en cualquier kernel compatible con stivale,
que luego se ejecuta con el nivel de privilegios más alto posible. Sí, ¿qué podría salir mal, verdad?

Debido a que me niego a revelar código de mala calidad, no habrá soporte para stivale en **Easyboot**. Y si sigues mi consejo,
ningún desarrollador aficionado de SO debería utilizarlo por sí mismo.
