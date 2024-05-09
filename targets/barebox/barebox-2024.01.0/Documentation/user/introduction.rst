Introduction
============

This is the barebox user manual, which describes how to configure, compile
and run barebox on embedded systems.

barebox (just barebox, not *the* barebox) is a bootloader designed for
embedded systems. It runs on a variety of architectures including
x86, ARM, MIPS, PowerPC and others.

barebox aims to be a versatile and flexible bootloader, not only for
booting embedded Linux systems, but also for initial hardware bringup and
development. barebox is highly configurable to be suitable as a full-featured
development binary as well as for lean production systems.
Just like busybox is the Swiss Army Knife for embedded Linux,
barebox is the Swiss Army Knife for bare metal, hence the name.

.. _feedback:

Feedback
--------

For sending patches, asking for help and giving general feedback you are
always welcome to write an e-mail to the barebox mailing list. Most of the
discussion of barebox takes place here:

http://lists.infradead.org/mailman/listinfo/barebox/

Mails sent to the barebox mailing list are archived on
`lore.barebox.org <https://lore.barebox.org/barebox/>`_.

Patch series sent there can be applied with `b4 <https://pypi.org/project/b4/>`_ ::

   git config b4.midmask https://lore.barebox.org/%s
   git config b4.linkmask https://lore.barebox.org/%s
   b4 shazam -M https://lore.barebox.org/$messageid # replace with link

There's also an IRC channel: #barebox on Libera Chat
