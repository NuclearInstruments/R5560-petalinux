#!/bin/sh

S=/mnt/sdcard
echo Upgrading FPGA firmware
echo Mounting EMMC memory boot partition on ${S}

mount /dev/mmcblk0p1 ${S}

cp /home/root/firmware.fpga ${S}/.
rm /home/root/firmware.fpga

sync

umount ${S}



