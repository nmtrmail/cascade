#!/bin/bash

# Generate image
dd if=/dev/zero of=/sdcard.img bs=1 count=0 seek=4G

# Create loopback
LOOPBACK_ROOT=$(losetup --show -f --sizelimit 4G /sdcard.img)
echo "Root loopback device is ${LOOPBACK_ROOT}"

# Create the partitions
# 10MB A2 (contains SPL)
# 32MB fat32 (type b) (u-boot, zImage, bitstream and device-tree)
# Rest allocated to ext4
sfdisk ${LOOPBACK_ROOT} -uS << EOF
,32M,b
,10M,A2
;
EOF

# Rescan partitions after modifying the partition table
partprobe ${LOOPBACK_ROOT}

sync

echo "Load loopback image"
dd if=/output_files/u-boot-with-spl.sfp of=${LOOPBACK_ROOT}p2

echo "Format u-boot FAT partition"
mkfs.vfat ${LOOPBACK_ROOT}p1
mkdir /fatmnt
mount ${LOOPBACK_ROOT}p1 /fatmnt
cp /output_files/zImage /fatmnt
cp /output_files/bitfile.rbf /fatmnt
cp /output_files/soc_system.dtb /fatmnt
cp /output_files/boot.scr /fatmnt
umount /fatmnt

echo "Get rootfs ready"
# Architectural chroot and install required packages
cp /etc/resolv.conf /builder/rootfs/etc/resolv.conf
mount -o bind /dev /builder/rootfs/dev
mount -o bind /proc /builder/rootfs/proc
mount -o bind /sys /builder/rootfs/sys
mount -o bind /dev/pts /builder/rootfs/dev/pts

chroot /builder/rootfs /bin/bash -c "apt-get update;export DEBIAN_FRONTEND=noninteractive;ln -fs /usr/share/zoneinfo/America/Los_Angeles /etc/localtime;apt-get install -y tzdata;dpkg-reconfigure --frontend noninteractive tzdata"
chroot /builder/rootfs /bin/bash -c "apt-get install -y sudo systemd systemd-sysv udev openssh-server build-essential cmake git python3 python3-dev python3-pip python3-venv flex iputils-ping ntp ntpdate bison rsync;apt-get autoclean;sudo apt-get clean;apt-get autoremove"
chroot /builder/rootfs /bin/bash -c "echo 'y' | /usr/local/sbin/unminimize"

# Generate username/password fpga/fpga
chroot /builder/rootfs /bin/bash -c "useradd -m -p '\$6\$xJFN8Cb2\$LII5axOW4ByQL8P3ctYuZC7vpwETFBTXJgcV806P2/UjYNV2hxXPeta6uNX/GbF.FD4M/xMM3ABGE1P7KMhyt1' -s /bin/bash fpga"
chroot /builder/rootfs /bin/bash -c "sudo usermod -aG sudo fpga"
chmod -R a+rw /builder/rootfs/root

# Build cascade
mkdir -p /builder/rootfs/cascade
mount -o bind /cascade_dir /builder/rootfs/cascade
chroot /builder/rootfs /bin/bash -c "mkdir /cascade_build;cd /cascade_build; cmake /cascade;make -j 4;make install"

#umount /builder/rootfs/dev/pts
#umount /builder/rootfs/dev
#umount /builder/rootfs/proc
#umount /builder/rootfs/sys
#umount /builder/rootfs/cascade

echo "Format ext4 rootfs"
#mkfs.ext4 ${LOOPBACK_ROOT}p3
#mkdir /extmnt
#mount ${LOOPBACK_ROOT}p3 /extmnt
#rsync -axHAXW --progress /builder/rootfs/* /extmnt/

#umount /extmnt

#losetup -d ${LOOPBACK_ROOT}

cp sdcard.img /output_files/sdcard.img