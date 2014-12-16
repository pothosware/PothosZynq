#!/bin/bash

#puts bootgen in path
source /opt/Xilinx/SDK/2014.3.1/settings64.sh

set -x #echo on

#make device tree
dtc  -I dts -O dtb -o ./BOOT/devicetree.dtb ./system.dts

#make BOOT.bin from files specified in bif
bootgen -image ./fsbl.bif -w -o i ./BOOT/BOOT.bin
