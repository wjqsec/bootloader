barebox environment
===================

This driver provides an environment for barebox from the devicetree.

Required properties:

* ``compatible``: should be ``barebox,environment``
* ``device-path``: path to the device environment is on

Optional properties:

* ``file-path``: path to a file in the device named by device-path

The device-path is a multistring property. The first string should contain
a nodepath to the node containing the physical device of the environment or
a nodepath to a partition described by the OF partition binding.
The subsequent strings are of the form <type>:<options> to further describe
the path to the environment. Supported values for <type>:

``partname``:<partname>
  This describes a partition on a device. <partname> can
  be the label for MTD partitions, the number for DOS
  partitions (beginning with 0) or the name for GPT partitions.

If the *environment* is located in a GPT partition, use
``6C3737F2-07F8-45D1-AD45-15D260AAB24D`` as partition type GUID.

The file-path is the name of a file located in a FAT filesystem on the
device named in device-path.  This filesystem will be mounted and the
environment loaded from the file's location in the directory tree.

Example:

.. code-block:: none

  environment {
  	compatible = "barebox,environment";
  	device-path = &flash, "partname:barebox-environment";
  };

  environment@1 {
  	compatible = "barebox,environment";
  	device-path = &mmc, "partname:1";
  	file-path = "barebox.env";
  };
