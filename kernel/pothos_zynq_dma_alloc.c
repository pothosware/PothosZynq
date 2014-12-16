// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "pothos_zynq_dma_module.h"
#include <linux/uaccess.h> //copy_to/from_user
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

long pothos_zynq_dma_buffs_alloc(pothos_zynq_dma_device_t *data, const pothos_zynq_dma_alloc_t *user_config, pothos_zynq_dma_alloc_t *allocs)
{
    struct platform_device *pdev = data->pdev;

    //are we already allocated?
    if (allocs->buffs != NULL) return -EBUSY;

    //copy the buffer into kernel space
    pothos_zynq_dma_alloc_t alloc_args;
    if (copy_from_user(&alloc_args, user_config, sizeof(pothos_zynq_dma_alloc_t)) != 0)
    {
        return -EACCES;
    }

    //check the sentinel
    if (alloc_args.sentinel != POTHOS_ZYNQ_DMA_SENTINEL) return -EINVAL;

    //copy the dma buffers array into kernel space
    allocs->num_buffs = alloc_args.num_buffs;
    allocs->buffs = devm_kzalloc(&pdev->dev, alloc_args.num_buffs*sizeof(pothos_zynq_dma_buff_t), GFP_KERNEL);
    if (copy_from_user(allocs->buffs, alloc_args.buffs, alloc_args.num_buffs*sizeof(pothos_zynq_dma_buff_t)) != 0)
    {
        return -EACCES;
    }

    //allocate dma buffers
    for (size_t i = 0; i < allocs->num_buffs; i++)
    {
        dma_addr_t phys_addr = 0;
        size_t size = allocs->buffs[i].bytes;
        void *virt_addr = dma_zalloc_coherent(&pdev->dev, size, &phys_addr, GFP_KERNEL);
        allocs->buffs[i].paddr = phys_addr;
        allocs->buffs[i].kaddr = virt_addr;
        allocs->buffs[i].uaddr = NULL; //filled by user with mmap
    }

    //copy the allocation results back to the user ioctl buffer
    if (copy_to_user(alloc_args.buffs, allocs->buffs, alloc_args.num_buffs*sizeof(pothos_zynq_dma_buff_t)) != 0)
    {
        return -EACCES;
    }

    return 0;
}

long pothos_zynq_dma_buffs_free(pothos_zynq_dma_device_t *data, pothos_zynq_dma_alloc_t *allocs)
{
    struct platform_device *pdev = data->pdev;

    //already freed earlier
    if (allocs->buffs == NULL) return 0;

    //free dma buffers
    for (size_t i = 0; i < allocs->num_buffs; i++)
    {
        if (allocs->buffs[i].kaddr == NULL) continue; //alloc failed eariler
        dma_free_coherent(&pdev->dev, allocs->buffs[i].bytes, allocs->buffs[i].kaddr, allocs->buffs[i].paddr);
    }

    //free the dma buffer structures
    devm_kfree(&pdev->dev, allocs->buffs);

    //clear
    allocs->num_buffs = 0;
    allocs->buffs = NULL;
    return 0;
}
