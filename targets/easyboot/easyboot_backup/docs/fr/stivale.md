Prise en charge du Stivale dans Easyboot
========================================

Cela n’arrivera jamais, jamais. Ce protocole de démarrage comporte de très mauvais choix de conception et constitue un risque de
sécurité énorme.

Premièrement, les noyaux stivale ont un en-tête ELF, mais d'une manière ou d'une autre, vous êtes censé savoir que l'en-tête n'est
pas valide ; rien, absolument rien n'implique dans l'en-tête que ce n'est pas un noyau SysV ABI valide. Dans Multiboot, il doit y
avoir des octets magiques au début du fichier pour que vous puissiez détecter le protocole ; il n'y a rien de tel dans stivale /
stivale2. (L'ancre ne vous aide pas, car cela peut se produire *n'importe où* dans le fichier, vous devez donc en fait *rechercher
tout le fichier* pour être sûr qu'il n'est pas compatible avec stivale.)

Deuxièmement, il utilise des sections ; qui, selon la spécification ELF (voir [page 46](https://www.sco.com/developers/devspecs/gabi41.pdf))
sont facultatifs et aucun chargeur ne devrait s'en soucier. Les chargeurs utilisent la vue Execution et non la vue Linking.
Implémenter l'analyse de section simplement à cause de ce protocole est une surcharge insensée dans n'importe quel chargeur, où les
ressources système sont généralement déjà rares.

Troisièmement, les en-têtes de section sont situés à la fin du fichier. Cela signifie que pour détecter sivale, vous devez charger
le début du fichier, analyser les en-têtes ELF, puis charger la fin du fichier et analyser les en-têtes de section, puis charger
quelque part à partir du milieu du fichier pour obtenir la liste de balises réelle. . C’est la pire solution possible. Et encore
une fois, rien n'indique qu'un chargeur devrait faire cela, vous devez donc le faire pour tous les noyaux juste pour découvrir que
le noyau n'utilise pas stivale. Cela ralentit également la détection de *tous les autres* protocoles de démarrage, ce qui est
inacceptable.

La liste de balises est activement interrogée par les processeurs d'application et le noyau peut appeler le code du chargeur de
démarrage à tout moment, ce qui signifie que vous ne pouvez tout simplement pas récupérer la mémoire du chargeur de démarrage,
sinon un crash est garanti. Cela va à l'encontre de la philosophie d'**Easyboot**.

Le pire, c'est que le protocole s'attend à ce que les chargeurs de démarrage injectent du code dans n'importe quel noyau compatible
avec stivale, qui est ensuite exécuté avec le niveau de privilèges le plus élevé possible. Ouais, qu'est-ce qui pourrait mal
tourner, n'est-ce pas?

Parce que je refuse de diffuser du code de mauvaise qualité, il n'y aura pas de support stivale dans **Easyboot**. Et si vous suivez
mon conseil, aucun développeur de système d’exploitation amateur ne devrait jamais l’utiliser pour son propre bien.
