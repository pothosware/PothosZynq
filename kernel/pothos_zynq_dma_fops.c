// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "pothos_zynq_dma_module.h"
#include <linux/fs.h> //struct file
#include <linux/io.h> //ioctl
#include <linux/mm.h> //mmap
#include <linux/slab.h> //kmalloc
#include <linux/uaccess.h> //copy_to/from_user

long pothos_zynq_dma_ioctl_chan(pothos_zynq_dma_user_t *user, const pothos_zynq_dma_setup_t *user_config)
{
    //copy the buffer into kernel space
    pothos_zynq_dma_setup_t setup_args;
    if (copy_from_user(&setup_args, user_config, sizeof(pothos_zynq_dma_setup_t)) != 0) return -EACCES;

    //check the sentinel
    if (setup_args.sentinel != POTHOS_ZYNQ_DMA_SENTINEL) return -EINVAL;

    //set the engine pointer
    if (setup_args.engine_no >= user->module->num_engines) return -EINVAL;
    user->engine = user->module->engines + setup_args.engine_no;

    //set the channel pointer
    if (setup_args.direction == POTHOS_ZYNQ_DMA_MM2S) user->chan = &user->engine->mm2s_chan;
    else if (setup_args.direction == POTHOS_ZYNQ_DMA_S2MM) user->chan = &user->engine->s2mm_chan;
    else return -EINVAL;

    //check the claimed status
    if (user->chan->claimed)
    {
        user->chan = NULL;
        return -EBUSY;
    }
    user->chan->claimed = 1;

    return 0;
}

long pothos_zynq_dma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    pothos_zynq_dma_user_t *user = (pothos_zynq_dma_user_t *)filp->private_data;

    //associate the user data with a channel
    switch (cmd)
    {
    case POTHOS_ZYNQ_DMA_SETUP: return pothos_zynq_dma_ioctl_chan(user, (pothos_zynq_dma_setup_t *)arg);
    }

    //check user configuration for these
    if (user->engine == NULL) return -ENODEV;
    if (user->chan == NULL) return -ENODEV;
    switch (cmd)
    {
    case POTHOS_ZYNQ_DMA_ALLOC: return pothos_zynq_dma_ioctl_alloc(user, (pothos_zynq_dma_alloc_t *)arg);
    case POTHOS_ZYNQ_DMA_FREE: return pothos_zynq_dma_ioctl_free(user);
    case POTHOS_ZYNQ_DMA_WAIT: return pothos_zynq_dma_ioctl_wait(user, (pothos_zynq_dma_wait_t *)arg);
    }

    return -EINVAL;
}

int pothos_zynq_dma_mmap(struct file *filp, struct vm_area_struct *vma)
{
    pothos_zynq_dma_user_t *user = (pothos_zynq_dma_user_t *)filp->private_data;
    const size_t size = vma->vm_end - vma->vm_start;
    const size_t offset = vma->vm_pgoff << PAGE_SHIFT;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    //The user passes in the physical address as the offset:
    #define try_map_buff(__b) if (offset != POTHOS_ZYNQ_DMA_REGS_OFF && offset == (__b).paddr) \
        return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot);
    for (size_t i = 0; i < user->chan->allocs.num_buffs; i++)
    {
        try_map_buff(user->chan->allocs.buffs[i]);
    }
    try_map_buff(user->chan->sgbuff);

    //Use a register alias point to map the registers in to user-space...
    //as the kernel has already iomapped the registers at offset 0.
    if (offset == POTHOS_ZYNQ_DMA_REGS_OFF)
    {
        const size_t register_alias = user->engine->regs_phys_addr + user->engine->regs_phys_size;
        return io_remap_pfn_range(vma, vma->vm_start, register_alias >> PAGE_SHIFT, size, vma->vm_page_prot);
    }

    return -EINVAL;
}

int pothos_zynq_dma_open(struct inode *inode, struct file *filp)
{
    //find the base of the data structure by seeing where cdev is stored
    pothos_zynq_dma_module_t *module = container_of(inode->i_cdev, pothos_zynq_dma_module_t, c_dev);

    //allocate user struct for this open file descriptor
    pothos_zynq_dma_user_t *user = kmalloc(sizeof(pothos_zynq_dma_user_t), GFP_KERNEL);
    if (user == NULL) return -EACCES;
    user->module = module;
    user->engine = NULL;
    user->chan = NULL;

    //now store it to private data for other methods
    filp->private_data = user;

    return 0;
}

int pothos_zynq_dma_release(struct inode *inode, struct file *filp)
{
    pothos_zynq_dma_user_t *user = (pothos_zynq_dma_user_t *)filp->private_data;
    if (user->chan != NULL)
    {
        pothos_zynq_dma_ioctl_free(user);
        user->chan->claimed = 0;
    }
    kfree(user);
    return 0;
}
