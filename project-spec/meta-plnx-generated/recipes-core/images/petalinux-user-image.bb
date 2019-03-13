DESCRIPTION = "PETALINUX image definition for Xilinx boards"
LICENSE = "MIT"

require recipes-core/images/petalinux-image-common.inc 

inherit extrausers
IMAGE_LINGUAS = " "

IMAGE_INSTALL = "\
		kernel-modules \
		i2c-tools \
		i2c-tools-dev \
		i2c-tools-misc \
		mtd-utils \
		tar \
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
		nibridge \
		rwmem \
		settings \
		"
EXTRA_USERS_PARAMS = "usermod -P root root;"
