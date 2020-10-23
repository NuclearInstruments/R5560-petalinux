#!/bin/sh
DIRS="/tmp/upgrade"
DIRM="/mnt/sdcard"
mkdir -p $DIRM/backup
mkdir -p $DIRM/backup/sys
mv $DIRM/image.ub $DIRM/backup/sys/iamge_prev.ub
mv $DIRS/image.ub $DIRM/image.ub
mv $DIRS/sys_version.json $DIRM/sys_version.json
