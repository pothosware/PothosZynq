# Build for Pothos Zynq DMA kernel module

## Build requirements

This build requires a checkout of the xilinx linux repository
which has been cross-compiled for ARM using the Xilinx SDK.

## Build instructions

```
source /opt/Xilinx/SDK/2014.3.1/settings64.sh
export CROSS_COMPILE=arm-xilinx-linux-gnueabi-
make ARCH=arm KDIR=path/to/linux-xlnx/
ls pothos_zynq_dma.ko #built kernel module
```
