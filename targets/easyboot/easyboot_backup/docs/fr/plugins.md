Plugins Easyboot
================

Par défaut, [Easyboot](https://gitlab.com/bztsrc/easyboot) démarre les noyaux compatibles Multiboot2 aux formats ELF et PE à partir
de la partition de démarrage. Si votre noyau utilise un format de fichier différent, un protocole de démarrage différent ou s'il
n'est pas situé sur la partition de démarrage, vous aurez besoin de plugins sur la partition de démarrage. Vous pouvez les trouver
dans le répertoire [src/plugins](../../src/plugins).

[[_TOC_]]

Installation
------------

Pour installer les plugins, copiez-les simplement dans le répertoire spécifié dans le paramètre `(indir)`, sous le sous-répertoire
`easyboot` à côté du fichier menu.cfg.

Par exemple:
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
$ easyboot bootpart disque.img
```

Compilation
-----------

*Il était évident dès le départ qu'ELF n'était pas adapté à cette tâche. C'est trop volumineux et trop complexe. Donc, à l'origine,
je voulais utiliser struct exec (le format UNIX a.out classique), mais malheureusement, les chaînes d'outils modernes ne peuvent
plus créer cela. J'ai donc décidé de créer mon propre format et mon propre éditeur de liens pour les plugins.*

Vous pouvez compiler la source du plugin avec n'importe quel compilateur croisé ANSI C standard dans un fichier objet ELF, mais vous
devrez ensuite utiliser l'éditeur de liens [plgld](../../src/misc/plgld.c) pour créer le binaire final. Il s'agit d'un éditeur de
liens indépendant de l'architecture, qui fonctionnera quel que soit le code machine pour lequel le plugin a été compilé. Le .plg
final n'est qu'une fraction de la taille du .o ELF à partir duquel il a été généré.

### API du plugin

La source C d'un plugin doit inclure le fichier d'en-tête `src/loader.h` et doit contenir une ligne `EASY_PLUGIN`. Celui-ci a un
paramètre, le type du plugin, suivi de la spécification de correspondance d'identifiant. Ce dernier est utilisé par le chargeur pour
déterminer quand utiliser ce plugin particulier.

Par example:

```c
#include "../loader.h"

/* octets magiques qui identifient un noyau Linux */
EASY_PLUGIN(PLG_T_KERNEL) {
   /* décalage  taille  type de match  octets magiques */
    { 0x1fe,        2,  PLG_M_CONST,  { 0xAA, 0x55, 0, 0 } },
    { 0x202,        4,  PLG_M_CONST,  { 'H', 'd', 'r', 'S' } }
};

/* point d'entrée, le prototype est défini par le type du plugin */
PLG_API void _start(uint8_t *buf, uint64_t size);
{
    /* préparer l'environnement pour un noyau Linux */
}
```

Les plugins peuvent utiliser plusieurs variables et fonctions, celles-ci sont toutes définies dans le fichier d'en-tête et liées au
runtime.

```c
uint32_t verbose;
```
Niveau de verbosité. Un plugin n'est autorisé à imprimer une sortie que si celle-ci est différente de zéro, à l'exception des
messages d'erreur. Plus sa valeur est grande, plus il faut imprimer de détails.

```c
uint64_t file_size;
```
La taille totale du fichier ouvert (voir `open` et `loadfile` ci-dessous).

```c
uint8_t *root_buf;
```
Lorsqu'un plugin de système de fichiers s'initialise, il contient les premiers 128 Ko de la partition (y compris, espérons-le, le
super bloc). Plus tard, un plugin de système de fichiers pourra réutiliser ce tampon de 128 Ko à n'importe quelle fin (cache FAT,
cache d'inodes, etc.)

```c
uint8_t *tags_buf;
```
Contient les balises Multiboot2. Un plugin du noyau peut analyser cela pour convertir les données fournies par le gestionnaire de
démarrage dans le format attendu par le noyau. Ce pointeur pointe vers le début du tampon.

```c
uint8_t *tags_ptr;
```
Ce pointeur pointe vers la fin du tampon des balises Multiboot2. Les plugins de balises peuvent ajouter de nouvelles balises ici et
ajuster ce pointeur.

```c
uint8_t *rsdp_ptr;
```
Pointe vers le pointeur RSDP ACPI.

```c
uint8_t *dsdt_ptr;
```
Pointe vers le blob de description matérielle DSDT (ou GUDT, FDT).

```c
efi_system_table_t *ST;
```
Sur les machines UEFI, pointe vers la table système EFI, sinon `NULL`.

```c
void memset(void *dst, uint8_t c, uint32_t n);
void memcpy(void *dst, const void *src, uint32_t n);
int  memcmp(const void *s1, const void *s2, uint32_t n);
```
Fonctions de mémoire obligatoires (le compilateur C peut générer des appels vers celles-ci, même s'il n'y a pas d'appel direct).

```c
void *alloc(uint32_t num);
```
Alloue `num` de pages (4 Ko) de mémoire. Les plugins ne doivent pas allouer beaucoup, ils doivent viser une empreinte mémoire
minimale.

```c
void free(void *buf, uint32_t num);
```
Libérez la mémoire précédemment allouée de `num` pages.

```c
void printf(char *fmt, ...);
```
Imprimez la chaîne formatée sur la console de démarrage.

```c
uint64_t pb_init(uint64_t size);
```
Démarre la barre de progression, `size` est la taille totale qu'elle représente. Renvoie combien d'octets vaut un pixel.

```c
void pb_draw(uint64_t curr);
```
Dessine la barre de progression pour la valeur actuelle. `curr` doit être compris entre 0 et la taille totale.

```c
void pb_fini(void);
```
Ferme la barre de progression, libère sa place à l'écran.

```c
void loadsec(uint64_t sec, void *dst);
```
Utilisé par les plugins du système de fichiers, charge un secteur du disque en mémoire. `sec` est le numéro du secteur, relatif à
la partition racine.

```c
void sethooks(void *o, void *r, void *c);
```
Utilisé par les plugins du système de fichiers, définit les hooks des fonctions open / read / close pour le système de fichiers de
la partition racine.

```c
int open(char *fn);
```
Ouvrez un fichier sur la partition racine (ou de démarrage) pour le lire, renvoie 1 en cas de succès. Un seul fichier peut être
ouvert à la fois. Lorsqu'il n'y a pas eu d'appel `sethooks` au préalable, il opère sur la partition de démarrage.

```c
uint64_t read(uint64_t offs, uint64_t size, void *buf);
```
Lit les données du fichier ouvert à la position de recherche `offs` dans la mémoire, renvoie le nombre d'octets réellement lus.

```c
void close(void);
```
Ferme le fichier ouvert.

```c
uint8_t *loadfile(char *path);
```
Chargez un fichier entièrement depuis la partition racine (ou de démarrage) dans une mémoire tampon nouvellement allouée et
décompressez-le de manière transparente si le plugin est trouvé. Taille renvoyée dans `file_size`.

```c
int loadseg(uint32_t offs, uint32_t filesz, uint64_t vaddr, uint32_t memsz);
```
Chargez un segment à partir du tampon du noyau. Cela vérifie si la mémoire `vaddr` est disponible et mappe le segment s'il est de
moitié supérieure. Le `offs` est le décalage du fichier, donc relatif au tampon du noyau. Si `memsz` est plus grand que `filesz`,
alors la différence est remplie de zéros.

```c
void _start(void);
```
Point d'entrée pour les plugins du système de fichiers (`PLG_T_FS`). Il doit analyser le superbloc dans `root_buf` et appeler
`sethooks`. En cas d'erreur, il devrait simplement revenir sans définir ses hooks.

```c
void _start(uint8_t *buf, uint64_t size);
```
Point d'entrée pour les plugins du noyau (`PLG_T_KERNEL`). Reçoit l'image du noyau en mémoire, il doit déplacer ses segments,
configurer l'environnement approprié et transférer le contrôle. Lorsqu'il n'y a pas d'erreur, il ne revient jamais.

```c
uint8_t *_start(uint8_t *buf);
```
Point d'entrée pour les plugins de décompresseur (`PLG_T_DECOMP`). Reçoit le tampon compressé (et sa taille dans `file_size`), et
doit renvoyer un nouveau tampon alloué avec les données non compressées (et définir également la taille du nouveau tampon dans
`file_size`). Il doit libérer l'ancien tampon (attention, `file_size` est en octets, mais free() attend une taille en pages). En
cas d'erreur, `file_size` ne doit pas être modifié et il doit renvoyer le tampon d'origine non modifié.

```c
void _start(void);
```
Point d'entrée pour les plugins de balises (`PLG_T_TAG`). Ils peuvent ajouter de nouvelles balises à `tags_ptr` et ajuster ce
pointeur à une nouvelle position alignée sur 8 octets.

### Fonctions locales

Les plugins peuvent utiliser des fonctions locales, cependant à cause d'un bug CLang, celles-ci *doivent* être déclarées comme
`static`. (Le bogue est que CLang génère des enregistrements PLT pour ces fonctions, même si l'indicateur "-fno-plt" est passé sur
la ligne de commande. L'utilisation de solutions de contournement `static` pour cela).

Spécification du format de fichier de bas niveau
------------------------------------------------

Dans le cas où quelqu'un souhaite écrire un plugin dans un langage autre que C (en Assembly par exemple), voici la description de
bas niveau du format de fichier.

Il est très similaire au format a.out. Le fichier se compose d’un en-tête de taille fixe, suivi de sections de longueur variable.
Il n'y a pas d'en-tête de section, les données de chaque section suivent directement les données de la section précédente dans
l'ordre suivant:

```
(entête)
(identifiant correspondant aux enregistrements)
(dossiers de déménagement)
(langage machine)
(données en lecture seule)
(données lisibles-inscriptibles initialisées)
```

Pour la première vraie section, le code machine, l'alignement est inclus. Pour toutes les autres sections, le remplissage est ajouté
à la taille de la section précédente.

ASTUCE: si vous transmettez un plugin comme argument unique à `plgld`, alors il vide les sections du fichier avec une sortie
similaire à `readelf -a` ou `objdump -xd`.

### Entête

Tous les nombres sont au format petit-boutiste (little endian) quelle que soit l'architecture.

| Décalage| Taille| Description                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     4 | octets magiques `EPLG`                                   |
|       4 |     4 | taille totale du fichier                                 |
|       8 |     4 | mémoire totale requise lorsque le fichier est chargé     |
|      12 |     4 | taille de la section de code                             |
|      16 |     4 | taille de la section de données en lecture seule         |
|      20 |     4 | point d'entrée du plugin                                 |
|      24 |     2 | code d'architecture (identique à celui d'ELF)            |
|      26 |     2 | nombre de dossiers de réinstallation                     |
|      28 |     1 | nombre d'enregistrements de correspondance d'identifiant |
|      29 |     1 | entrée GOT la plus référencée                            |
|      30 |     1 | révision du format de fichier (0 pour l'instant)         |
|      31 |     1 | type du plugin (1=système de fichiers, 2=noyau, 3=décompresseur, 4=tag) |

Le code d'architecture est le même que dans les en-têtes ELF, par exemple 62 = x86_64, 183 = Aarch64 et 243 = RISC-V.

Le type du plugin précise le prototype du point d'entrée, l'ABI est toujours SysV.

### Section Correspondance des identifiants

Cette section contient autant d'enregistrements suivants que spécifié dans le champ "nombre d'enregistrements de correspondance
d'identifiant" de l'en-tête.

| Décalage| Taille| Description                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     2 | décalage                                                 |
|       2 |     1 | taille                                                   |
|       3 |     1 | type                                                     |
|       4 |     4 | octets magiques correspondant                            |

Tout d'abord, le début du sujet est chargé dans un tampon. Un accumulateur est configuré, initialement avec un 0. Les décalages dans
ces enregistrements sont toujours relatifs à cet accumulateur et ils adressent cet octet dans le tampon.

Le champ Type indique comment interpréter le décalage. S'il s'agit d'un 1, alors le décalage plus l'accumulateur sont utilisés comme
valeur. Si c'est 2, alors une valeur d'octet de 8 bits est prise au décalage, 3 signifie prendre une valeur de mot de 16 bits et 4
signifie prendre une valeur de mot de 32 bits. 5 signifie prendre une valeur d'octet de 8 bits et y ajouter l'accumulateur, 6
signifie prendre une valeur de mot de 16 bits et y ajouter l'accumulateur, et 7 est le même mais avec une valeur de 32 bits. 8
recherchera les octets magiques depuis le dixième octet de l'accumulateur jusqu'à la fin du tampon par étapes de décalage, et s'il
est trouvé, renvoie le décalage correspondant comme valeur.

Si la taille est nulle, alors l'accumulateur est défini sur la valeur. Si la taille n'est pas nulle, alors ce nombre d'octets est
vérifié s'ils correspondent aux octets magiques donnés.

Par exemple, pour vérifier si un exécutable PE démarre par une instruction NOP:
```c
EASY_PLUGIN(PLG_T_KERNEL) {
   /* décalage  taille  type de match  octets magiques */
    { 0,            2,  PLG_M_CONST, { 'M', 'Z', 0, 0 } },  /* vérifier les octets magiques */
    { 60,           0,  PLG_M_DWORD, { 0, 0, 0, 0 } },      /* obtenir le décalage de l'en-tête PE par rapport à l'accumulateur */
    { 0,            4,  PLG_M_CONST, { 'P', 'E', 0, 0 } },  /* vérifier les octets magiques */
    { 40,           1,  PLG_M_DWORD, { 0x90, 0, 0, 0 } }    /* vérifier l'instruction NOP au point d'entrée */
};
```

### Section de réinstallation

Cette section contient autant d'enregistrements suivants que spécifié dans le champ "nombre de dossiers de réinstallation" de
l'en-tête.

| Décalage| Taille| Description                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     4 | décalage                                                 |
|       4 |     4 | type de réinstallation                                   |

Signification des bits dans le type:

| Depuis  | Vers  | Description                                              |
|--------:|------:|----------------------------------------------------------|
|       0 |     7 | symbole (0 - 255)                                        |
|       8 |     8 | Adressage relatif du PC                                  |
|       9 |     9 | Adressage indirect relatif du GOT                        |
|      10 |    13 | indice de masque immédiat (0 - 15)                       |
|      14 |    19 | début bit (0 - 63)                                       |
|      20 |    25 | fin bit (0 - 63)                                         |
|      26 |    31 | position du bit du drapeau d'adresse annulée (0 - 63)    |

Le champ offset est relatif à la magie dans l'en-tête du plugin, et il sélectionne un entier en mémoire où la relocalisation doit
être effectuée.

Le symbole indique quelle adresse utiliser. 0 signifie l'adresse BASE où le plugin a été chargé en mémoire, c'est-à-dire. l'adresse
de la magie de l'en-tête en mémoire. D'autres valeurs sélectionnent une adresse de symbole externe du GOT, définie dans le chargeur
ou dans un autre plugin, jetez un œil au tableau `plg_got` dans la source de plgld.c pour voir quelle valeur correspond à quel
symbole. Si le bit relatif GOT est 1, alors l'adresse de l'entrée GOT du symbole est utilisée, à la place de l'adresse réelle du
symbole.

Si le bit relatif du PC est 1, alors le décalage est d'abord soustrait de l'adresse (mode d'adressage relatif du pointeur
d'intrusion).

L'index de masque immédiat indique quels bits stockent l'adresse dans l'instruction. Si la valeur est 0, alors l'adresse est écrite
telle quelle sur le décalage, quelle que soit l'architecture. Pour x86_64, seul l'index 0 est autorisé. Pour ARM Aarch64 : 0 = tel
quel, 1 = 0x07ffffe0 (décalage vers la gauche de 5 bits), 2 = 0x07fffc00 (décalage vers la gauche de 10 bits), 3 = 0x60ffffe0 (avec
les instructions ADR/ADRP, l'immédiat est décalé et divisé en deux groupes de bits). Les architectures futures pourraient définir
des masques de bits immédiats plus nombreux et différents.

En utilisant le masque immédiat, les bits fin - début + 1 sont extraits de la mémoire et signés étendus. Cette valeur est ajoutée
à l'adresse (addend ajout, et en cas de références internes, l'adresse du symbole interne est également codée ici).

Si le bit d'indicateur d'adresse annulée n'est pas 0 et que l'adresse est positive, alors ce bit est effacé. Si l'adresse est
négative, alors ce bit est défini et l'adresse est annulée.

Enfin, les bits de début et de fin sélectionnent la partie de l'adresse à écrire dans l'entier sélectionné. Ceci définit également
la taille de la relocalisation, les bits en dehors de cette plage et ceux ne faisant pas partie du masque immédiat restent inchangés.

### Section des codes

Cette section contient des instructions machine pour l'architecture spécifiée dans l'en-tête et contient autant d'octets que
l'indique le champ de taille de code.

### Section de données en lecture seule

Il s'agit d'une section facultative, elle pourrait manquer. C'est aussi long que l'indique le champ de taille de section en lecture
seule dans l'en-tête. Toutes les variables constantes sont placées dans cette section.

### Section de données initialisées

Il s'agit d'une section facultative, elle pourrait manquer. S'il reste encore des octets dans le fichier après la section de code
(ou la section de données facultative en lecture seule), alors ces octets sont tous considérés comme la section de données. Si une
variable est initialisée avec une valeur non nulle, alors elle est placée dans cette section.

### Section BSS

Il s'agit d'une section facultative, elle pourrait manquer. Cette section n'est jamais stockée dans le fichier. Si le champ de
taille en mémoire est plus grand que le champ de taille de fichier dans l'en-tête, alors leur différence sera remplie de zéros en
mémoire. Si une variable n'est pas initialisée, ou initialisée à zéro, alors elle est placée dans cette section.
