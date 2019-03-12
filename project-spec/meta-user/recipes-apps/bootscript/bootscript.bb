#
# This file is the bootscript recipe.
#

SUMMARY = "Simple bootscript application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
CLEANBROKEN = "1"

SRC_URI = "file://mystartup \
		  "
inherit update-rc.d

INITSCRIPT_NAME = "mystartup"
INITSCRIPT_PARAMS = "start 99 S ."

S = "${WORKDIR}"

do_compile[noexec] = "1"

do_install() {
	     install -d ${D}${sysconfdir}/init.d
	     install -m 0755 ${S}/mystartup ${D}${sysconfdir}/init.d/mystartup
}

FILES_${PN} += "${sysconfdir}/*"
