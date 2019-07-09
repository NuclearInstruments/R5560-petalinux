#
# This file is the libzmq recipe.
#

SUMMARY = "ZeroMQ Library"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://libzmq.so.5.2.2 \
	"

S = "${WORKDIR}"

INSANE_SKIP_${PN} = "ldflags"
INHIBIT_PACKAGE_STRIP = "1"
INHIBIT_SYSROOT_STRIP = "1"

do_install() {
	     install -d ${D}/${libdir}
	     oe_soinstall ${S}/libzmq.so.5.2.2 ${D}${libdir}
	     chrpath -d ${D}${libdir}/libzmq.so.5.2.2
}
