Stivale támogatás az Easybootban
================================

Sosem lesz, soha. Ez a boot protokoll tele van valami elképesztően elhibázott dönténsekkel és orbitális biztonsági kockázat.

Először is, egy stivale kernelnek ELF fejléce van, mégis valahogy tudnod kéne róla, hogy ez a fejléc nem érvényes; arra persze
semmi, de tényleg semmi sem utal a fejlécben, hogy a kernel nem is SysV ABI-t használ. Multiboot alatt a fájl elején kell lennie
egy mágikus bájtsornak, ami alapján detektálható; na semmi ilyesmi nincs a stivale / stivale2 protokollban. (Van ugyan horgony, de
az nem segít, mert bárhol előfordulhat, így *az egész fájlt végig kell nyálazni*, csakhogy biztos lehess benne, mégsem stivale
kompatíbilis.)

Másodszor, szekciókat használ; az ELF specifikáció szerint (lásd [46. oldal](https://www.sco.com/developers/devspecs/gabi41.pdf))
ezek opcionálisak és semelyik betöltőnek sem szabadna foglalkozni velük. A betöltők ugyanis az Execution View-t használják, és nem
pedig a Linking View-t. A szekciókezelés leimplementálása csak emiatt az egy protokoll miatt őrületes többletet jelent bármelyik
betöltő számára, mikor az erőforrásaik egyébként is roppantul szűkösek.

Harmadszor, a szekciófejlécek a fájl végén találhatók. Ez azt jelenti, hogy ahhoz, hogy detektálhasd a stivale-t, először is be
kell olvasni a fájl elejéről az ELF fejlécet, értelmezni kell azt, majd be kell olvasni a fájl legvégéről a szekció fejlécet és
azt is értelmezni, végül pedig be kell olvasni valahonnan a fájl kellős közepéről is magát a teglistát. Ennél rosszabb megoldást
elképzelni sem lehet. És mégegyszer, az égvilágon semmi sem utal arra, hogy egy betöltőnek ezt meg kell tennie, így minden kernel
esetében meg kell tenni, csakhogy eldönthessük, nem is használ stivale-t. Ez lelassítja az *összes többi* boot protokoll
detektálását is, ami egyszerűen elfogadhatatlan.

A teglistát aktívan monitorozzák az applikációs processzormagok, valamint a kernel bármikor belehívhat a betöltő kódjába, így
lehetetlen a betöltő memóriaterületét felszabadítani. Ha mégis megtennénk, akkor garantált, hogy fejreáll az egész. Ez élesen
szembemegy az **Easyboot** filozófiájával.

De a legrosszab rész az, hogy a protokoll elvárja a betöltőtől, hogy kódot injektáljon minden stivale kompatíbilis kernelbe,
amit aztán azok a lehető legmagasabb privilégiumi szinten futtatnak. Ja, mi baj lehet ebből, igaz?

Mivel nem vagyok hajlandó szar kódot kiadni a kezeim közül, ezért nem lesz stivale támogatás az **Easyboot**-ban. És ha
megfogadjátok a tanácsom, akkor minden más hobby OS fejlesztő is a saját jól megfontolt érdekében messze elkerüli.
