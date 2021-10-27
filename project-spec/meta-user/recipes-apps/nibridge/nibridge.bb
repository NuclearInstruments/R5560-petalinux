#
# This file is the nibridge recipe.
#

SUMMARY = "Simple nibridge application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
CLEANBROKEN = "1"

INSANE_SKIP_${PN} += "file-rdeps"

DEPENDS += " libzmq"

SRC_URI = "file://nibridge.c \
           file://zmq.h \
           file://zhelpers.h \
           file://zmq_utils.h \
		   file://libzmq.so.5.2.2 \
			file://Makefile \
		  "

#

S = "${WORKDIR}"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 nibridge ${D}${bindir}
}
