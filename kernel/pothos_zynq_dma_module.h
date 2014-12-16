// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include "pothos_zynq_dma_common.h"
#include <linux/wait.h> //wait_queue_head_t
#include <linux/cdev.h> //character device

/*!
 * Per-device configuration, allocations, mappings...
 */
typedef struct
{
    // track opens to share this structure
    atomic_long_t use_count;

    //devfs registration
    dev_t dev_num;
    struct cdev c_dev;
    struct class *cl;

    //wait queue for implementing interrupt waits
    wait_queue_head_t irq_wait;
    unsigned long long irq_count;

    //the platform device from probe
    struct platform_device *pdev;

    //dma engine register space
    phys_addr_t regs_phys_addr; //!< hardware address of the registers from device tree
    size_t regs_phys_size; //!< size in bytes of the registers from device tree
    void __iomem *regs_virt_addr; //!< virtual mapping of register space from ioremap

    //dma buffer allocations
    pothos_zynq_dma_alloc_t s2mm_allocs;
    pothos_zynq_dma_alloc_t mm2s_allocs;

} pothos_zynq_dma_device_t;

//! Register an interrupt handler -- called by probe
int pothos_zynq_dma_register_irq(unsigned int irq, pothos_zynq_dma_device_t *data);

//! Remove an interrupt handler -- called by unprobe
void pothos_zynq_dma_unregister_irq(unsigned int irq, pothos_zynq_dma_device_t *data);

//! IOCTL access for user to control allocations
long pothos_zynq_dma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

//! Map DMA and device registers into userspace
int pothos_zynq_dma_mmap(struct file *filp, struct vm_area_struct *vma);

//! The user calls open on the device node
int pothos_zynq_dma_open(struct inode *inode, struct file *filp);

//! The user calls close on the device node
int pothos_zynq_dma_release(struct inode *inode, struct file *filp);

//! Allocate DMA buffers from IOCTL configuration struct
long pothos_zynq_dma_buffs_alloc(pothos_zynq_dma_device_t *data, const pothos_zynq_dma_alloc_t *user_config, pothos_zynq_dma_alloc_t *allocs);

//! Free DMA buffers allocated from buffs alloc
long pothos_zynq_dma_buffs_free(pothos_zynq_dma_device_t *data, pothos_zynq_dma_alloc_t *allocs);

//! Wait on DMA completion from IOCTL configuration struct
long pothos_zynq_dma_wait(pothos_zynq_dma_device_t *data, const pothos_zynq_dma_wait_t *user_config, pothos_zynq_dma_alloc_t *allocs);
