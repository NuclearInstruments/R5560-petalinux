#
# This file is the settings recipe.
#

SUMMARY = "Simple settings application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
CLEANBROKEN = "1"

SRC_URI = "file://mount_sd.sh \
          file://umount_sd.sh \
          file://upgrade_fpga.sh \
          file://upgrade_rootfs.sh \                    
          file://dropbear_rsa_host_key \
		  "

S = "${WORKDIR}"

do_compile[noexec] = "1"


do_install() {
	     install -d ${D}${bindir}
	     install -d ${D}${sysconfdir}/dropbear	     
	     install -m 0755 ${S}/mount_sd.sh ${D}${bindir}/mount_sd.sh
	     install -m 0755 ${S}/umount_sd.sh ${D}${bindir}/umount_sd.sh
	     install -m 0755 ${S}/upgrade_fpga.sh ${D}${bindir}/upgrade_fpga.sh
	     install -m 0755 ${S}/upgrade_rootfs.sh ${D}${bindir}/upgrade_rootfs.sh	     	     
	     install -m 0755 ${S}/dropbear_rsa_host_key ${D}${sysconfdir}/dropbear/dropbear_rsa_host_key	     	     	     
}
