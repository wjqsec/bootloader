Stivale Support in Easyboot
===========================

Not going to happen, never ever. This boot protocol has some very serious bad design choices and it is a huge security risk.

First, stivale kernels have an ELF header, yet somehow you are supposed to know that the header isn't valid; nothing, absolutely
nothing implies in the header that it's not a valid SysV ABI kernel. In Multiboot there must be some magic bytes at the beginning
of the file so that you can detect the protocol; there's nothing like that in stivale / stivale2. (The anchor doesn't help you,
because that may occur *anywhere* in the file, so actually must *search the whole file* to be sure it's not stivale compatible.)

Second, it uses sections; which according to the ELF specification (see [page 46](https://www.sco.com/developers/devspecs/gabi41.pdf))
are optional and no loader should care about. Loaders use the Execution View and not the Linking View. Implementing section parsing
just because of this one protocol is an insane overhead in any loader, where the system resources are usually already scarce.

Third, section headers are located at the end of the file. This means that in order to detect stivale, you must load the beginning
of the file, parse ELF headers, then load the end of the file and parse section headers, and then load somewhere from the middle
of the file to get the actual tag list. This is the worst possible solution there could be. And again, there's absolutely nothing
indicating that a loader should do this, so you must do it for all kernels just to find out the kernel doesn't use stivale. This
slows down the detection of *all the other* boot protocols as well, which is unacceptable.

The tag list is actively polled by the application processors, and the kernel might call into boot loader code any time, meaning
you simply cannot reclaim the boot loader's memory otherwise a crash is guaranteed. This goes against **Easyboot**'s philosphy.

The worst part is, the protocol expects boot loaders to inject code into any stivale compatible kernel, which is then executed on
the highest priviledge level possible. Yeah, what could go wrong, right?

Because I refuse to give poor quality code out of my hands, there'll be no stivale support in **Easyboot**. And if you take my
advice, no hobby OS developers should ever use it either for their own sake.
