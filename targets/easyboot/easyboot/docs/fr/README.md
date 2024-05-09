Démarrer les noyaux avec Easyboot
=================================

[Easyboot](https://gitlab.com/bztsrc/easyboot) est un gestionnaire de démarrage tout-en-un et un créateur d'image disque amorçable
qui peut charger divers noyaux de système d'exploitation et noyaux compatibles Multiboot2 dans différents formats.

[[_TOC_]]

Installation
------------

```
 easyboot [-v|-vv] [-s <mb>] [-b <mb>] [-u <guid>] [-p <t> <u> <i>] [-e] [-c] <indir> <outfile>

  -v, -vv         augmenter la verbosité / validation
  -s <mb>         définir la taille de l'image disque en mégaoctets (par défaut à 35 Mo)
  -b <mb>         définir la taille de la partition de démarrage en mégaoctets (par défaut à 33 Mo)
  -u <guid>       définir l'identifiant unique de la partition de démarrage (par défaut, aléatoire)
  -p <t> <u> <i>  ajouter une partition supplémentaire (tapez guid, guid unique, fichier image)
  -e              ajouter El Torito Boot Catalog (prise en charge du démarrage sur CDROM EFI)
  -c              créez toujours un nouveau fichier image même s'il existe
  <indir>         utiliser le contenu de ce répertoire pour la partition de démarrage
  <outfile>       image de sortie ou nom du fichier de périphérique
```

L'outil **Easyboot** crée une image disque amorçable nommée « (outfile) » à l'aide de la table de partitionnement GUID avec une
seule partition formatée en FAT32 et nommée « Partition système EFI » (ESP en abrégé). Le contenu de cette partition est extrait du
`(indir)` que vous fournissez. Vous devez placer un simple fichier de configuration en texte brut dans ce répertoire appelé
`easyboot/menu.cfg`. Avec les fins de ligne NL ou CRLF, vous pouvez facilement les modifier avec n'importe quel éditeur de texte.
En fonction de votre configuration, vous pourriez également avoir besoin de quelques [plugins](plugins.md) dans ce répertoire
appelé `easyboot/*.plg`.

L'image peut également être démarrée sur Raspberry Pi et fonctionnera dans qemu; mais pour démarrer sur une vraie machine, vous
aurez besoin d'autres fichiers de firmware `bootcode.bin`, `fixup.dat`, `start.elf` et d'un fichier .dtb dans le répertoire `(indir)`,
ceux-ci peuvent être téléchargés à partir du [dépôt officiel](https://github.com/raspberrypi/firmware/tree/master/boot) du Raspberry
Pi.

L'outil dispose également de quelques indicateurs de ligne de commande facultatifs : `-s (mb)` définit la taille globale de l'image
disque générée en mégaoctets, tandis que `-b (mb)` définit la taille de la partition de démarrage en mégaoctets. Il est évident que
le premier doit être plus grand que le second. Si elle n'est pas spécifiée, la taille de la partition est calculée à partir de la
taille du répertoire donné (33 Mo au minimum, le plus petit FAT32 qui puisse exister) et la taille du disque est par défaut de 2 Mo
plus grande que cela (en raison de l'alignement et de l'espace nécessaire pour le partitionnement). tableau). S'il y a un écart de
plus de 2 Mo entre ces deux valeurs de taille, vous pouvez utiliser des outils tiers tels que `fdisk` pour ajouter plus de
partitions à l'image à votre guise (ou voir `-p` ci-dessous). Si vous souhaitez une disposition prévisible, vous pouvez également
spécifier l'identifiant unique de la partition de démarrage (UniquePartitionGUID) avec l'indicateur `-u <guid>`.

En option, vous pouvez également ajouter des partitions supplémentaires avec l'indicateur `-p`. Cela nécessite 3 arguments:
(PartitionTypeGUID), (UniquePartitionGUID) et le nom du fichier image qui contient le contenu de la partition.

L'indicateur `-e` ajoute El Torito Boot Catalog à l'image générée, afin qu'elle puisse être démarrée non seulement comme une clé
USB mais également comme un CDROM EFI.

Si `(outfile)` est un fichier de périphérique (par exemple `/dev/sda` sous Linux, `/dev/disk0` sous BSD et `\\.\PhysicalDrive0` sous
Windows), alors il ne crée pas de GPT ni ESP, au lieu de cela, localise ceux déjà existants sur l'appareil. Il copie toujours tous
les fichiers de « (indir) » sur la partition de démarrage et installe les chargeurs. Cela fonctionne également si `(outfile)` est un
fichier image qui existe déjà (dans ce cas, utilisez l'indicateur `-c` pour toujours créer d'abord un nouveau fichier image vide).

Configuration
-------------

Le fichier `easyboot/menu.cfg` peut contenir les lignes suivantes (très similaire à la syntaxe de grub.cfg, vous pouvez trouver un
exemple de fichier de configuration [ici](menu.cfg)):

### Commentaires

Toutes les lignes commençant par un `#` sont considérées comme des commentaires et ignorées jusqu'à la fin de la ligne.

### Niveau de verbosité

Vous pouvez définir le niveau de verbosité en utilisant une ligne commençant par `verbose`.

```
verbose (0-3)
```

Cela indique au chargeur la quantité d'informations à imprimer sur la console de démarrage. Verbose 0 signifie totalement silencieux
(par défaut) et verbeux 3 videra les segments du noyau chargés et le code machine au point d'entrée.

### Framebuffer

Vous pouvez demander une résolution d'écran spécifique avec la ligne commençant par `framebuffer`. Le format est le suivant:

```
framebuffer (largeur) (hauteur) (bits par pixel) [#(premier plan)] [#(l'arrière plan)] [#(la barre de progression)]
```

**Easyboot** configurera un framebuffer pour vous, même si cette ligne n'existe pas (800 x 600 x 32bpp par défaut). Mais si cette
ligne existe, alors il tentera de définir la résolution spécifiée. Les modes palettes ne sont pas pris en charge, donc les bits par
pixel doivent être d'au moins 15.

Si le quatrième paramètre de couleur facultatif est donné, alors il doit être en notation HTML commençant par un dièse suivi de 6
chiffres hexadécimaux, RRGGBB. Par exemple, `#ff0000` est d'un rouge vif et `#007f7f` est d'un cyan plus foncé. Il définit le premier
plan ou, en d'autres termes, la couleur de la police. De même, si le cinquième paramètre de couleur facultatif est donné, il doit
également être en notation HTML et définit la couleur d'arrière-plan. Le dernier paramètre de couleur facultatif l'est également et
définit la couleur d'arrière-plan de la barre de progression.

Lorsque les couleurs ne sont pas données, la valeur par défaut est des lettres blanches sur fond noir et l'arrière-plan de la barre
de progression est gris foncé.

### Option de démarrage par défaut

La ligne commençant par `default` définit quelle entrée de menu doit être démarrée sans interaction de l'utilisateur après le délai
d'attente spécifié.

```
default (index d'entrée de menu) (délai msec)
```

L'index d'entrée de menu est un nombre compris entre 1 et 9. Le délai d'attente est en millisecondes, donc 1000 donne une seconde.

### Alignement des menus

Les lignes commençant par `menualign` modifient la façon dont les options de démarrage sont affichées.

```
menualign ("vertical"|"horizontal")
```

Par défaut, le menu est horizontal, que vous pouvez modifier en vertical.

### Options de démarrage

Vous pouvez spécifier jusqu'à 9 entrées de menu avec des lignes commençant par `menuentry`. Le format est le suivant:

```
menuentry (icône) [étiquette]
```

Pour chaque icône, un fichier `easyboot/(icône).tga` sur la partition de démarrage doit exister. L'image doit être codée en longueur
et mappée en couleurs [format Targa](../en/TGA.txt), car c'est la variante la plus compressée (les trois premiers octets du fichier
doivent être `0`, `1` et `9` dans cet ordre, voir Type de données 9 dans la spécification). Ses dimensions doivent être exactement
de 64 pixels de haut et 64 pixels de large.

Pour enregistrer dans ce format depuis GIMP, sélectionnez d'abord "Image > Mode > Indexé...", dans la fenêtre contextuelle, réglez
"Nombre maximum de couleurs" sur 256. Sélectionnez ensuite "Fichier > Exporter sous...", entrez un nom de fichier qui se termine
par `.tga`, et dans la fenêtre pop-up, cochez "Compression RLE". Pour un outil de conversion en ligne de commande, vous pouvez
utiliser ImageMagick, `convert (n'importe quel fichier image) -colors 256 -compress RLE icône.tga`.

Le paramètre facultatif label sert à afficher les informations de version ou de version ASCII dans le menu, et non pour les chaînes
arbitraires, donc pour économiser de l'espace, UTF-8 n'est pas pris en charge à moins que vous ne fournissiez également
`easyboot/font.sfn`. (Une police UNICODE nécessite beaucoup de stockage, même si la
[Scalable Screen Font](https://gitlab.com/bztsrc/scalable-font2) est très efficace, elle fait toujours environ 820 Ko. En revanche,
le fichier unicode.pf2 de GRUB est bien plus grande, environ 2 392 Ko, bien que les deux polices contiennent environ 55 600 glyphes
dans des bitmaps 8 x 16 et 16 x 16. La police ASCII intégrée contient des bitmaps 8 x 16 et 96 glyphes uniquement.)

Toutes les lignes qui suivent une ligne `menuentry` appartiendront à cette entrée de menu, sauf lorsque cette ligne est une autre
ligne de menuentry. Pour plus de commodité, vous pouvez utiliser des blocs comme dans GRUB, mais ce ne sont que du sucre syntaxique.
Les accolades sont traitées comme des caractères d'espacement. Vous pouvez les omettre et utiliser des onglets à la place, comme
dans un script Python ou Makefile si telle est votre préférence.

Par exemple
```
menuentry FreeBSD backup
{
    kernel bsd.old/boot
}
```

### Sélectionner une partition

La ligne commençant par `partition` sélectionne une partition GPT. Doit être précédé d'une ligne `menuentry`.

```
partition (UUID unique de la partition)
```

Cette partition sera utilisée comme système de fichiers racine pour l'option de démarrage. Le noyau ainsi que les modules seront
chargés depuis cette partition, et selon le protocole de démarrage, ils seront également transmis au noyau. La partition spécifiée
peut résider sur un disque différent de celui du disque de démarrage. **Easyboot** effectuera une itération sur tous les disques
partitionnés GPT pendant le démarrage pour la localiser.

Pour plus de commodité, la partition est également recherchée sur la ligne `kernel` (voir ci-dessous). Si la ligne de commande de
démarrage donnée contient une chaîne `root=(UUID)` ou `root=*=(UUID)`, alors il n'est pas nécessaire d'avoir une ligne `partition`
distincte.

Lorsque la partition n'est pas explicitement spécifiée alors le noyau ainsi que les modules sont chargés depuis la partition de
démarrage.

### Spécifier un noyau

La ligne commençant par « kernel » indique quel fichier doit être démarré et avec quels paramètres. Doit être précédé d'une ligne
`menuentry`.

```
kernel (chemin d'accès à votre fichier noyau) (arguments de ligne de commande de démarrage facultatifs)
```

Le chemin doit pointer vers un fichier existant, un binaire exécutable du noyau, et il doit s'agir d'un chemin UTF-8 absolu sur la
partition racine (ou de démarrage). Si le noyau ne se trouve pas dans le répertoire racine de la partition, alors le séparateur de
répertoire est toujours `/`, même sur les systèmes UEFI. Si le nom contient un espace, alors celui-ci doit être échappé avec `\`.
Le chemin peut être suivi d'arguments de ligne de commande, séparés par un espace. Pour les noyaux compatibles Multiboot2, ces
arguments de ligne de commande seront transmis dans la balise *Ligne de commande de démarrage* (type 1). Ils ne seront pas modifiés
par **Easyboot**, ni analysés sauf recherchés pour le spécificateur de partition.

Par défaut, **Easyboot** peut démarrer des noyaux compatibles Multiboot2 aux formats ELF64 et PE32+/COFF (et sur les systèmes UEFI,
les applications UEFI également). Normalement, ce protocole n'autorise pas les noyaux de moitié supérieure, mais **Easyboot** viole
un peu le protocole d'une manière qui ne brise pas les noyaux normaux non de moitié supérieure. Si vous souhaitez démarrer un autre
noyau, vous aurez besoin d'un [plugin](plugins.md) de chargeur de noyau.

REMARQUE: contrairement à GRUB, où vous devez utiliser des commandes spéciales comme "linux" ou "multiboot" pour sélectionner le
protocole de démarrage, ici il n'y a qu'une seule commande et le protocole est automatiquement détecté à partir de votre noyau au
moment de l'exécution.

### Chargement d'autres modules

Vous pouvez charger des fichiers arbitraires (disques virtuels initiaux, pilotes du noyau, etc.) avec le noyau en utilisant des
lignes commençant par `module`. Doit être précédé d'une ligne `menuentry`. Notez que cette ligne peut être répétée plusieurs fois
dans chaque entrée de menu. Si le protocole de démarrage prend en charge un initrd, alors la toute première ligne du `module` est
considérée comme l'initrd.

```
module (chemin d'accès à un fichier) (arguments de ligne de commande du module facultatifs)
```

Le chemin doit pointer vers un fichier existant et il doit s'agir d'un chemin UTF-8 absolu sur la partition racine (ou de démarrage).
Il peut être suivi d'arguments de ligne de commande, séparés par un espace. Si le fichier est compressé et qu'il existe un
[plugin](plugins.md) de décompression pour celui-ci, alors le module sera décompressé de manière transparente. Les informations sur
ces modules chargés (et non compressés) seront transmises à un noyau compatible Multiboot2 dans les balises *Modules* (type 3).

Le cas particulier est si le module commence par les octets `DSDT`, `GUDT` ou `0xD00DFEED`. Dans ces cas, le fichier ne sera pas
ajouté au Les balises *Modules*, à la place la table ACPI seront corrigées pour que ses pointeurs DSDT pointent vers le contenu de
ce fichier. Avec ça vous pouvez facilement remplacer la table ACPI d'un BIOS bogué par une table fournie par l'utilisateur.

### Logo de démarrage

Vous pouvez également afficher un logo au centre de l'écran lorsque l'option de démarrage est sélectionnée si vous placez une ligne
commençant par `bootsplash`. Doit être précédé d'une ligne `menuentry`.

```
bootsplash [#(l'arrière plan)] (chemin d'accès à un tga fichier)
```

La couleur d'arrière-plan est facultative et doit être en notation HTML commençant par un dièse suivi de 6 chiffres hexadécimaux,
RRGGBB. Si le premier argument ne commence pas par `#`, alors un argument de chemin est supposé.

Le chemin doit pointer vers un fichier existant et il doit s'agir d'un chemin UTF-8 absolu sur la partition de démarrage (PAS racine).
L'image doit être dans un format Targa codé en longueur et avec mappage de couleurs, tout comme les icônes de menu. Les dimensions
peuvent être tout ce qui tient à l'écran.

### Prise en charge multicœur

Pour démarrer le noyau sur tous les cœurs du processeur à la fois, spécifiez la directive `multicore`. Doit être précédé d'une
ligne `menuentry` et nécessite également l'installation du plugin [smp](../../src/plugins/smp.c).

```
multicore
```

Dépannage
---------

Si vous rencontrez des problèmes, exécutez simplement avec l'indicateur `easyboot -vv`. Cela effectuera une validation et affichera
les résultats de manière détaillée au moment de la création de l'image. Sinon, ajoutez `verbose 3` à `easyboot/menu.cfg` pour
obtenir des messages détaillés sur le temps de démarrage.

Si vous voyez la chaîne `PMBR-ERR` dans le coin supérieur gauche avec un fond rouge, cela signifie que votre processeur est très
ancien et ne prend pas en charge le mode long 64 bits ou que le secteur de démarrage n'a pas pu amorcer le chargeur. Cela peut se
produire uniquement sur les machines BIOS, cela ne peut jamais se produire avec UEFI ou sur RaspberryPi.

| Messages                            | Descriptif                                                                           |
|-------------------------------------|--------------------------------------------------------------------------------------|
| `Booting [X]...`                    | indique que l'option de démarrage (index d'menuentry) X a été choisie                |
| `Loading 'X' (Y bytes)...`          | le fichier X de longueur Y est en cours de chargement                                |
| `Parsing kernel...`                 | noyau a été trouvé, détectant maintenant son format                                  |
| `Starting X boot...`                | montre que le chargeur de démarrage du système X a été détecté                       |
| `Starting X kernel...`              | montre que le noyau du système X a été détecté                                       |
| `Transfering X control to Y`        | indique que le point d'entrée en mode X à l'adresse Y est sur le point d'être appelé |

Si vous rencontrez des problèmes après avoir vu ce dernier message, cela signifie que le problème s'est produit dans la procédure de
démarrage du système d'exploitation, et non dans le chargeur **Easyboot**, vous devrez donc consulter la documentation du système
d'exploitation concerné. Sinon, n'hésitez pas à ouvrir un [problème](https://gitlab.com/bztsrc/easyboot/-/issues) sur gitlab.

### Multiboot1

Plugins requis: [grubmb1](../../src/plugins/grubmb1.c)

### Multiboot2

C'est le plus flexible, avec plusieurs variantes prises en charge via des plugins:

- ELF64 ou PE32+ avec Multiboot2 simplifié: aucun plugin requis
- ELF32 avec Multiboot2 simplifié et point d'entrée 32 bits: [elf32](../../src/plugins/elf32.c)
- a.out (struct exec) avec Multiboot2 simplifié et point d'entrée 32 bits: [aout](../../src/plugins/aout.c)
- Multiboot2 compatible GRUB avec point d'entrée 32 bits: [grubmb2](../../src/plugins/grubmb2.c)

Notez la différence: le [Multiboot2 simplifié](ABI.md) ne nécessite aucune balise intégrée, prend en charge les demi-noyaux
supérieurs, un point d'entrée 64 bits propre avec des paramètres transmis conformément aux ABI Multiboot2, SysV et fastcall
également.

D'un autre côté, [Multiboot2 compatible GRUB](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html) nécessite des
balises intégrées, ne prend pas en charge les noyaux de moitié supérieure ni 64 bits, le point d'entrée est toujours 32 bits et
les paramètres sont passés dans `eax`, `ebx` uniquement.

### Windows

Aucun plugin requis, mais vous devez configurer [SecureBoot](secureboot.md).

```
menuentry win {
  kernel EFI/Microsoft/BOOT/BOOTMGRW.EFI
}
```

### Linux

Plugins requis: [linux](../../src/plugins/linux.c), [ext234](../../src/plugins/ext234.c)

Si le noyau n'est pas placé sur la partition de démarrage, vous pouvez utiliser la ligne de commande pour spécifier la partition
racine.

```
menuentry linux {
  kernel vmlinuz-linux root=PARTUUID=01020304-0506-0708-0a0b0c0d0e0f1011
}
```

### OpenBSD

Plugins requis: [obsdboot](../../src/plugins/obsdboot.c), [ufs44](../../src/plugins/ufs44.c)

```
menuentry openbsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot
}
```

AVERTISSEMENT! N'utilisez pas le plugin [elf32](../../src/plugins/elf32.c) si vous démarrez OpenBSD! Son `boot` prétend à tort
être un ELF avec un point d'entrée SysV ABI 32 bits, mais en réalité il a une entrée en mode réel 16 bits.

### FreeBSD

Plugins requis: [fbsdboot](../../src/plugins/fbsdboot.c), [ufs2](../../src/plugins/ufs2.c)

Sur les systèmes BIOS existants, spécifiez le chargeur `boot`.

```
menuentry freebsd {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel boot/boot
}
```

Sur les machines UEFI, utilisez le chargeur `loader.efi` sur la partition de démarrage (aucun plugin nécessaire).

```
menuentry freebsd {
  kernel boot/loader.efi
}
```

Si votre système de fichiers racine est ZFS, copiez ce fichier (`boot` sur le BIOS, `loader.efi` sur UEFI) dans `(indir)` et NE
spécifiez PAS une partition racine.

### FreeDOS

Plugins requis: [fdos](../../src/plugins/fdos.c)

Déplacez les fichiers de FreeDOS vers `(indir)` (FreeDOS utilisera la partition de démarrage comme partition racine).

```
menuentry freedos {
  kernel KERNEL.SYS
}
```

Si le démarrage s'arrête après l'impression des droits d'auteur et de `- InitDisk`, alors le noyau FreeDOS a été compilé sans le
support FAT32. Téléchargez un autre noyau, avec `f32` dans son nom.

### ReactOS

Plugins requis: [reactos](../../src/plugins/reactos.c)

```
menuentry reactos {
  kernel FREELDR.SYS
}
```

### MenuetOS

Plugins requis: [menuet](../../src/plugins/menuet.c)

```
menuentry menuetos {
  kernel KERNEL.MNT
  module CONFIG.MNT
  module RAMDISK.MNT
}
```

Pour désactiver la configuration automatique, ajoutez `noauto` à la ligne de commande.

### KolibriOS

Plugins requis: [kolibri](../../src/plugins/kolibri.c)

```
menuentry kolibrios {
  kernel KERNEL.MNT syspath=/rd/1/ launcher_start=1
  module KOLIBRI.IMG
  module DEVICES.DAT
}
```

Le plugin fonctionne également sur les machines UEFI, mais vous pouvez également utiliser `uefi4kos.efi` sur la partition de
démarrage (et aucun plugin n'est requis).

### SerenityOS

Plugins requis: [grubmb1](../../src/plugins/grubmb1.c)

```
menuentry serenityos {
  kernel boot/Prekernel
  module boot/Kernel
}
```

### Haiku

Plugins requis: [grubmb1](../../src/plugins/grubmb1.c), [befs](../../src/plugins/befs.c)

```
menuentry haiku {
  partition 01020304-0506-0708-0a0b0c0d0e0f1011
  kernel system/packages/haiku_loader-r1~beta4_hrev56578_59-1-x86_64.hpkg
}
```

Sur les machines UEFI, utilisez le chargeur `haiku_loader.efi` sur la partition de démarrage (aucun plugin nécessaire).
