// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <linux/ioctl.h>
#include <linux/types.h>

//! The mmap offset used to specify the register space
#define POTHOS_ZYNQ_DMA_REGS_OFF 0

//! The size in bytes of the register space of interest
#define POTHOS_ZYNQ_DMA_REGS_SIZE 1024

//! Change this when the structure changes
#define POTHOS_ZYNQ_DMA_SENTINEL 0xab0d1d87

//! Constant for memory map to stream
#define POTHOS_ZYNQ_DMA_MM2S 0

//! Constant for stream to memory map
#define POTHOS_ZYNQ_DMA_S2MM 1

/*!
 * A descriptor for a single DMA buffer.
 */
typedef struct
{
    size_t bytes; //!< the number of bytes to allocate
    size_t paddr; //!< the physical address of the memory
    void *kaddr; //!< the kernel address of the memory
    void *uaddr; //!< the userspace address of the memory
} pothos_zynq_dma_buff_t;

/*!
 * The IOCTL structured used to request allocations.
 * The addresses will be filled in on successful allocations with ioctl.
 * The user must call mmap with paddr as the offset to fill in the uaddr.
 */
typedef struct
{
    unsigned int sentinel; //!< A expected word for ABI compatibility checks
    size_t engine_no; //!< Engine number specifies the DMA engine number
    size_t direction; //!< Channel direction specifies MM2S or S2MM
} pothos_zynq_dma_setup_t;

/*!
 * The IOCTL structured used to request allocations.
 * The addresses will be filled in on successful allocations with ioctl.
 * The user must call mmap with paddr as the offset to fill in the uaddr.
 */
typedef struct
{
    unsigned int sentinel; //!< A expected word for ABI compatibility checks
    size_t chan_index; //!< Channel index specifies the DMA engine number
    size_t chan_dir; //!< Channel directions specifies MM2S or S2MM
    size_t num_buffs; //!< The number of DMA buffers
    pothos_zynq_dma_buff_t *buffs; //!< An array of DMA buffers
    pothos_zynq_dma_buff_t sgbuff; //!< The buffer for the SG table
} pothos_zynq_dma_alloc_t;

/*!
 * The IOCTL structured used for wait completions (direction-independent).
 * The index should indicate the head entry in a scatter/gather table.
 */
typedef struct
{
    unsigned int sentinel; //!< A expected word for ABI compatibility checks
    size_t sgindex; //!< The index into the scatter/gather table to check
    long timeout_us; //!< the timeout to wait for completion in microseconds
} pothos_zynq_dma_wait_t;


//! Setup the DMA channel for the open file descriptor
#define POTHOS_ZYNQ_DMA_SETUP _IOW('p', 1, pothos_zynq_dma_setup_t *)

//! Allocate DMA buffers and the scatter/gather table
#define POTHOS_ZYNQ_DMA_ALLOC _IOWR('p', 2, pothos_zynq_dma_alloc_t *)

//! Free all allocations performed by POTHOS_ZYNQ_DMA_ALLOC
#define POTHOS_ZYNQ_DMA_FREE _IO('p', 3)

//! Wait with a timeout for a scatter/gather entry to complete
#define POTHOS_ZYNQ_DMA_WAIT _IOW('p', 4, pothos_zynq_dma_wait_t *)

/***********************************************************************
 * Register constants for AXI DMA v7.1
 *
 * Reference material:
 * https://github.com/Xilinx/linux-xlnx/blob/master/drivers/dma/xilinx/xilinx_axidma.c
 * http://www.xilinx.com/support/documentation/ip_documentation/axi_dma/v7_1/pg021_axi_dma.pdf
 **********************************************************************/
/* Register Offsets */
#define XILINX_DMA_MM2S_DMACR_OFFSET 0x00
#define XILINX_DMA_MM2S_DMASR_OFFSET 0x04
#define XILINX_DMA_MM2S_CURDESC_OFFSET 0x08
#define XILINX_DMA_MM2S_TAILDESC_OFFSET 0x10
#define XILINX_DMA_SG_CTL_OFFSET 0x2C
#define XILINX_DMA_S2MM_DMACR_OFFSET 0x30
#define XILINX_DMA_S2MM_DMASR_OFFSET 0x34
#define XILINX_DMA_S2MM_CURDESC_OFFSET 0x38
#define XILINX_DMA_S2MM_TAILDESC_OFFSET 0x40

/* General register bits definitions */
#define XILINX_DMA_CR_RESET_MASK	0x00000004 /* Reset DMA engine */
#define XILINX_DMA_CR_RUNSTOP_MASK	0x00000001 /* Start/stop DMA engine */
#define XILINX_DMA_SR_HALTED_MASK	0x00000001 /* DMA channel halted */
#define XILINX_DMA_SR_IDLE_MASK	0x00000002 /* DMA channel idle */
#define XILINX_DMA_XR_IRQ_IOC_MASK	0x00001000 /* Completion interrupt */
#define XILINX_DMA_XR_IRQ_DELAY_MASK	0x00002000 /* Delay interrupt */
#define XILINX_DMA_XR_IRQ_ERROR_MASK	0x00004000 /* Error interrupt */
#define XILINX_DMA_XR_IRQ_ALL_MASK	0x00007000 /* All interrupts */
#define XILINX_DMA_XR_DELAY_MASK	0xFF000000 /* Delay timeout counter */
#define XILINX_DMA_XR_COALESCE_MASK	0x00FF0000 /* Coalesce counter */
#define XILINX_DMA_DELAY_SHIFT	24 /* Delay timeout counter shift */
#define XILINX_DMA_COALESCE_SHIFT	16 /* Coalesce counter shift */
#define XILINX_DMA_DELAY_MAX	0xFF /* Maximum delay counter value */
#define XILINX_DMA_COALESCE_MAX	0xFF /* Max coalescing counter value */
#define XILINX_DMA_RX_CHANNEL_OFFSET	0x30 /* S2MM Channel Offset */

/* BD definitions for AXI Dma */
#define XILINX_DMA_BD_STS_ALL_MASK	0xF0000000
#define XILINX_DMA_BD_SOP	0x08000000 /* Start of packet bit */
#define XILINX_DMA_BD_EOP	0x04000000 /* End of packet bit */

/* Feature encodings */
#define XILINX_DMA_FTR_HAS_SG	0x00000100 /* Has SG */
#define XILINX_DMA_FTR_HAS_SG_SHIFT	8 /* Has SG shift */

/* Optional feature for dma */
#define XILINX_DMA_FTR_STSCNTRL_STRM	0x00010000

/* Delay loop counter to prevent hardware failure */
#define XILINX_DMA_RESET_LOOP	1000000
#define XILINX_DMA_HALT_LOOP	1000000

/* Scatter/Gather descriptor */
typedef struct xilinx_dma_desc_sg
{
    uint32_t next_desc; /* 0x00 */
    uint32_t pad1; /* 0x04 */
    uint32_t buf_addr; /* 0x08 */
    uint32_t pad2; /* 0x0C */
    uint32_t pad3; /* 0x10 */
    uint32_t pad4; /* 0x14 */
    uint32_t control; /* 0x18 */
    uint32_t status; /* 0x1C */
    uint32_t app_0; /* 0x20 */
    uint32_t app_1; /* 0x24 */
    uint32_t app_2; /* 0x28 */
    uint32_t app_3; /* 0x2C */
    uint32_t app_4; /* 0x30 */
} __attribute__ ((aligned (64))) xilinx_dma_desc_t;
