# Petalinux for R5560 DAQ (Both A and B)

This petalinux compile the image.ub and eventually also boot.bin to load to the DAQ flash of the Nuclear Instruments / CAEN R5560

This code works only on the DAQ and not on the base of the instrument. This code works only for R5560 and is not compatible with DT5560 or R5560SE


# R5560 DAQ FLASH MEMORY ORGANIZATION
The R5560 has a ZYNQ-7030 SoC.
The R5560 has two flash:
 * A QSPI flash that store the boot firmware and the BOOT.BIN file (which include just the fsbl and u-boot)
 * A 8 Gbyte EMMC that store both user files and image.ub. Image.ub contais kernel, device tree and rootfs (as ramfs)

A bootstrap the instruments boot from QSPI. 
USER SHOULD NEVER REPROGRAM THE QSPI WITH NEW BOON.BIN

The uboot load the linux from the EMMC looking for the image.ub file
In order to upgrade the whole linux (that means the rootfs and the kernel) is sufficient to upload a new image.ub in the folder 
/mnt/sdcard that is automatically mounted at boot.
That is possible via sftp with username/password: root 
Remember to sync the file system (with sync command) before reboot or your system will probably not boot any more

The rootfs is a ramfs: it means that it is boundle at compiler time by petalinux and no modification to the rootfs are possible. 
This is to increase the reliability of the instrument.
If user wants to add software or modify defualt, petalinux must be used to regenerate the image.ub

The folder /mnt/sdcard is mounted in rw indeed it is possible to store user file, script and new application there

# Compile petalinux project

In order to compiler petalinux project an ubuntu 16.04 with Petalinux 2018.1 is required
1) Clone this repo
2) Source petalinux script source /opt/Xilinx/petalinux/settings.sh
3) enter in the repo folder
4) petalinux-build

At the end you will find the image.ub in the images/linux folder


# Generation of upgrade packet

The Caen/NI open hardware upgrade tool allows to automate the upgrade process of R5560 DAQ
In order to generate an upgrade packet from a just build petalinux system run the sript generate_rsys.sh that is present in the repo main folder.
You fill find and rsus file called R5550_Upgrade_2021-11-30.rsys that is a zip that the Caen/NI open hardware upgrade tool can process and manage




