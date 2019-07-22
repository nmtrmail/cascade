#!/bin/bash

# Build the kernel
cd /builder/kernel &&\
        ARCH=arm make socfpga_defconfig &&\
        ARCH=arm make zImage -j4

cp arch/arm/boot/zImage /output_files/zImage

# Build u-boot
cd /builder/uboot && make mrproper && make socfpga_de0_nano_soc_defconfig && make -j${MAKE_PARALLELISM}
cp /builder/uboot/u-boot-with-spl.sfp /output_files/u-boot-with-spl.sfp
/builder/uboot/tools/mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "Cascade DE10-nano boot script" -d /u-boot.script /boot.scr
cp /boot.scr /output_files/boot.scr