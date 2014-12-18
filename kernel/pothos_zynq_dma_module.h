// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "pothos_zynq_dma_common.h"
#include <linux/wait.h> //wait_queue_head_t
#include <linux/cdev.h> //character device
#include <linux/interrupt.h> //irq types

#define MODULE_NAME "pothos_zynq_dma"

/*!
 * Data for a single DMA channel (either direction)
 */
typedef struct
{
    //dma buffer allocations
    pothos_zynq_dma_alloc_t allocs;

    //scatter gather buffer
    pothos_zynq_dma_buff_t sgbuff;

    //scatter gather table
    xilinx_dma_desc_t *sgtable;

    //memory mapped registers
    void __iomem *register_ctrl;
    void __iomem *register_stat;

    //interrupt configuration
    unsigned int irq_number;
    wait_queue_head_t irq_wait;
    unsigned long long irq_count;
    int irq_registered;

} pothos_zynq_dma_chan_t;

/*!
 * Data for a single DMA engine
 */
typedef struct
{
    //the platform device from probe
    struct platform_device *pdev;

    //dma engine register space
    phys_addr_t regs_phys_addr; //!< hardware address of the registers from device tree
    size_t regs_phys_size; //!< size in bytes of the registers from device tree
    void __iomem *regs_virt_addr; //!< virtual mapping of register space from ioremap

    //channel data - both directions
    pothos_zynq_dma_chan_t mm2s_chan;
    pothos_zynq_dma_chan_t s2mm_chan;
} pothos_zynq_dma_engine_t;

/*!
 * Data for the DMA module
 */
typedef struct
{
    // available engines in this system
    pothos_zynq_dma_engine_t *engines;
    size_t num_engines;

    //devfs registration
    dev_t dev_num;
    struct cdev c_dev;
    struct class *cl;

} pothos_zynq_dma_module_t;

/*!
 * Data for a user data structure.
 * This gets configured by an IOCTL after open.
 */
typedef struct
{
    pothos_zynq_dma_module_t *module;
    pothos_zynq_dma_engine_t *engine;
    pothos_zynq_dma_chan_t *chan;
} pothos_zynq_dma_user_t;

//! Interrupt handler for either direction
irqreturn_t pothos_zynq_dma_irq_handler(int irq, void *data);

//! IOCTL access for user to control allocations
long pothos_zynq_dma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

//! Map DMA and device registers into userspace
int pothos_zynq_dma_mmap(struct file *filp, struct vm_area_struct *vma);

//! The user calls open on the device node
int pothos_zynq_dma_open(struct inode *inode, struct file *filp);

//! The user calls close on the device node
int pothos_zynq_dma_release(struct inode *inode, struct file *filp);

//! Setup channel specification from IOCTL configuration struct
long pothos_zynq_dma_ioctl_chan(pothos_zynq_dma_user_t *user, const pothos_zynq_dma_setup_t *user_config);

//! Allocate DMA buffers from IOCTL configuration struct
long pothos_zynq_dma_ioctl_alloc(pothos_zynq_dma_user_t *user, pothos_zynq_dma_alloc_t *user_config);

//! Free DMA buffers allocated from buffs alloc
long pothos_zynq_dma_ioctl_free(pothos_zynq_dma_user_t *user);

//! Wait on DMA completion from IOCTL configuration struct
long pothos_zynq_dma_ioctl_wait(pothos_zynq_dma_user_t *user, const pothos_zynq_dma_wait_t *user_config);
