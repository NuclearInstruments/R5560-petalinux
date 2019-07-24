DESCRIPTION = "PETALINUX image definition for Xilinx boards"
LICENSE = "MIT"

require recipes-core/images/petalinux-image-common.inc 

inherit extrausers
IMAGE_LINGUAS = " "

IMAGE_INSTALL = "\
		kernel-modules \
		e2fsprogs \
		e2fsprogs-e2fsck \
		libext2fs \
		e2fsprogs-tune2fs \
		e2fsprogs-mke2fs \
		i2c-tools \
		i2c-tools-dev \
		i2c-tools-misc \
		mtd-utils \
		tar \
		util-linux-sfdisk \
		canutils \
		openssh-sftp-server \
		pciutils \
		run-postinsts \
		udev-extraconf \
		packagegroup-core-boot \
		packagegroup-core-ssh-dropbear \
		tcf-agent \
		bridge-utils \
		adcinit \
		bootscript \
		configdaemon \
		libzmq \
		nibridge \
		rwmem \
		settings \
		"
EXTRA_USERS_PARAMS = "usermod -P root root;"
