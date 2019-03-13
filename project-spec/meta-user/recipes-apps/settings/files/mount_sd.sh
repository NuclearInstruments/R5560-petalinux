#!/bin/sh

S=/mnt/sdcard

echo Mounting EMMC memory boot partition on ${S}

mount /dev/mmcblk0p1 ${S}


