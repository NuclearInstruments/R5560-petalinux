#
# This file is the configdaemon recipe.
#

SUMMARY = "Simple configdaemon application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
CLEANBROKEN = "1"

SRC_URI = "file://configdaemon.c \
           file://cJSON.h \
           file://cJSON.c \
	   file://Makefile \
		  "

S = "${WORKDIR}"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 configdaemon ${D}${bindir}
}
