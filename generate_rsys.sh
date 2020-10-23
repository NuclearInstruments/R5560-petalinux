#!/bin/bash

filename="R5550_Upgrade_$(date +%F).rsys"

echo "{\"model\":\"R5560\", \"type\":\"section\", \"os\":\"petalinux\", \"version\":\"$(date +%F)\"}" > "sys_version.json"
cp images/linux/image.ub .
tar -czvf $filename sys_version.json image.ub install.sh
rm image.ub
