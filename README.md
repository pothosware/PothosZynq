# DMA source and sink blocks for Xilinx Zynq FPGAs

The Pothos Zynq support package provides:

* DMA source and sink blocks for zero-copy buffer integration with a Pothos data flow
* a linux kernel module for interfacing with an AXI DMA engine in the programmable logic
* loopback examples in Vivado, Zynq boot files, and documentation to recreate the demo

## Layout

* kernel/ - the Pothos AXI DMA support linux kernel module
* driver/ - the Pothos AXI DMA userspace driver with C API
* blocks/ - Pothos framework DMA source and sink blocks
* loopback_microzed/ - example Vivado loopback project and boot files

## Dependencies

* Pothos library
* Zynq processor running linux

## Building

Configure, build, and install with CMake

## Using DMA blocks

```
insmod pothos_zynq_dma.ko
ls /dev/pothos_zynq_dma*
```

## Licensing information

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
