# i2c-bcm283x-rtdm

Real-Time I2C driver for the Broadcom BCM2835 (BCM2708) and BCM2836 (BCM2709) SoCs using the RTDM API.

## Introduction

This RTDM driver is intended to provide I2C communication between a Raspberry Pi running Xenomai 3.0 (or greater, potentially) and a maximum of two slave devices.

## Build

If you are unfamiliar with this technique, you can read the related documentation at https://www.kernel.org/doc/Documentation/kbuild/modules.txt.
This requires that you provide, as an argument, the location of the sources of an already built Xenomai-enabled kernel.

Start by retrieving the source of the driver.
```bash
$ git clone https://github.com/semulopez/i2c-bcm283x-rtdm.git
```

Then build against your already built kernel source.
```bash
$ make ARCH=arm CROSS_COMPILER=/path-to-gcc/prefix- KERNEL_DIR=/path-to-kernel-sources
```

Upon success, the release sub-folder will be populated with the generated kernel module.

## Usage

Copy the generated kernel module onto the target and load it with the following command.
```bash
$ sudo insmod i2c-bcm283x-rtdm.ko
```

The system log should display something like:
```
[   59.577534] bcm283x_i2c_rtdm_init: Starting driver ...
[   59.578867] bcm283x_i2c_rtdm_init: Device i2cdev0.0 registered without errors.
```

Once loaded, the driver will expose the device:
 * `/dev/rtdm/i2cdev0.0`

# Skin for i2c-bcm283x-rtmd driver
https://github.com/semulopez/rt-i2c-skin.git


## Credits

This code should be considered as a wrapper around the great user-space bcm2835 library written by Mike McCauley and available at http://www.airspayce.com/mikem/bcm2835/.

His code underwent only minor modifications, in order to make it compatible with kernel-space.

Both the original work and this driver are licensed under the GNU General Public License version 2 (GPLv2).

Comments, issues, and contributions are welcome.

Author: Sergio José Muñoz López (semulopez@gmail.com)
Tutor: Corrado Guarino Lo Bianco (guarino@ce.unipr.it)
Based on the work of: Nicolas Schurando (schurann@ext.essilor.com).
