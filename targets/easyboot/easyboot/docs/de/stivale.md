Stivale-Unterstützung in Easyboot
=================================

Das wird nicht passieren, niemals. Dieses Boot-Protokoll weist einige sehr schwerwiegende Designfehler auf und stellt ein großes
Sicherheitsrisiko dar.

Erstens haben Stivale-Kernel einen ELF-Header, aber irgendwie sollte man wissen, dass der Header ungültig ist; nichts, absolut
nichts deutet im Header darauf hin, dass es sich nicht um einen gültigen SysV-ABI-Kernel handelt. Bei Multiboot müssen am Anfang
der Datei einige magische Bytes stehen, damit Sie das Protokoll erkennen können. So etwas gibt es in stivale / stivale2 nicht.
(Der Anker hilft Ihnen nicht weiter, da dies *an einer beliebigen Stelle* in der Datei auftreten kann. Sie müssen also tatsächlich
*die gesamte Datei durchsuchen*, um sicherzustellen, dass sie nicht mit Stivale kompatibel ist.)

Zweitens verwendet es „sections“; die gemäß der ELF-Spezifikation (siehe [Seite 46](https://www.sco.com/developers/devspecs/gabi41.pdf))
optional sind und sich kein Loader darum kümmern sollte. Loader verwenden die Ausführungsansicht und nicht die Verknüpfungsansicht.
Die Implementierung der Abschnittsanalyse nur aufgrund dieses einen Protokolls ist in jedem Loader, in dem die Systemressourcen
normalerweise ohnehin schon knapp sind, ein wahnsinniger Mehraufwand.

Drittens befinden sich Abschnittsüberschriften am Ende der Datei. Das heißt, um Stivale zu erkennen, müssen Sie den Anfang der Datei
laden, ELF-Header analysieren, dann das Ende der Datei laden und Abschnittsheader analysieren und dann irgendwo in der Mitte der
Datei laden, um die eigentliche Tag-Liste zu erhalten . Das ist die schlechteste Lösung, die es geben kann. Und wiederum gibt es
absolut keinen Hinweis darauf, dass ein Loader dies tun sollte, Sie müssen es also für alle Kernel tun, nur um herauszufinden, dass
der Kernel Stivale nicht verwendet. Dies verlangsamt auch die Erkennung *aller anderen* Boot-Protokolle, was nicht akzeptabel ist.

Die Tag-Liste wird von den Anwendungsprozessoren aktiv abgefragt, und der Kernel kann jederzeit den Bootloader-Code aufrufen, was
bedeutet, dass Sie den Speicher des Bootloaders einfach nicht zurückgewinnen können, da sonst ein Absturz garantiert ist. Dies
widerspricht der Philosophie von **Easyboot**.

Das Schlimmste daran ist, dass das Protokoll von Bootloadern erwartet, dass sie Code in jeden Stivale-kompatiblen Kernel
einschleusen, der dann auf der höchstmöglichen Berechtigungsstufe ausgeführt wird. Ja, was könnte schief gehen, oder?

Da ich es ablehne, Code von schlechter Qualität aus meinen Händen zu geben, wird es in **Easyboot** keine Stivale-Unterstützung
geben. Und wenn Sie meinen Rat beherzigen, sollte es auch kein Hobby-OS-Entwickler aus Selbstzweck nutzen.
