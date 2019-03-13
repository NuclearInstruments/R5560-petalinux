#!/bin/sh

S=/mnt/sdcard
echo Upgrading ROOTFS image.ub firmware
echo Mounting EMMC memory boot partition on ${S}

mount /dev/mmcblk0p1 ${S}

cp /home/root/image.ub ${S}/.
rm /home/root/image.ub
sync

umount ${S}



