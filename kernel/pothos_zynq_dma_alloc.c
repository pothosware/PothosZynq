// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "pothos_zynq_dma_module.h"
#include <linux/uaccess.h> //copy_to/from_user
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

static void pothos_zynq_dma_buff_alloc(struct platform_device *pdev, pothos_zynq_dma_buff_t *buff)
{
    dma_addr_t phys_addr = 0;
    int rc = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
    if (rc)
        dev_err(&pdev->dev, "Error dma_set_coherent_mask() = %d.\n", rc);
    void *virt_addr = dma_zalloc_coherent(&pdev->dev, buff->bytes, &phys_addr, GFP_KERNEL);
    buff->paddr = phys_addr;
    buff->kaddr = virt_addr;
    buff->uaddr = NULL; //filled by user with mmap
}

long pothos_zynq_dma_ioctl_alloc(pothos_zynq_dma_user_t *user, pothos_zynq_dma_alloc_t *user_config)
{
    pothos_zynq_dma_chan_t *chan = user->chan;
    struct platform_device *pdev = user->engine->pdev;

    //copy the buffer into kernel space
    pothos_zynq_dma_alloc_t alloc_args;
    if (copy_from_user(&alloc_args, user_config, sizeof(pothos_zynq_dma_alloc_t)) != 0) return -EACCES;

    //check the sentinel
    if (alloc_args.sentinel != POTHOS_ZYNQ_DMA_SENTINEL) return -EINVAL;

    //are we already allocated?
    if (chan->allocs.buffs != NULL) return -EBUSY;

    //copy the dma buffers array into kernel space
    chan->allocs.num_buffs = alloc_args.num_buffs;
    chan->allocs.buffs = devm_kzalloc(&pdev->dev, alloc_args.num_buffs*sizeof(pothos_zynq_dma_buff_t), GFP_KERNEL);
    if (copy_from_user(chan->allocs.buffs, alloc_args.buffs, alloc_args.num_buffs*sizeof(pothos_zynq_dma_buff_t)) != 0) return -EACCES;

    //allocate dma buffers
    for (size_t i = 0; i < chan->allocs.num_buffs; i++)
    {
        pothos_zynq_dma_buff_alloc(pdev, chan->allocs.buffs+i);
    }

    //allocate SG table
    chan->sgbuff.bytes = sizeof(xilinx_dma_desc_t)*chan->allocs.num_buffs;
    pothos_zynq_dma_buff_alloc(pdev, &chan->sgbuff);
    chan->sgtable = (xilinx_dma_desc_t *)chan->sgbuff.kaddr;

    //copy the allocation results back to the user ioctl buffer
    if (copy_to_user(alloc_args.buffs, chan->allocs.buffs, alloc_args.num_buffs*sizeof(pothos_zynq_dma_buff_t)) != 0) return -EACCES;
    if (copy_to_user(&user_config->sgbuff, &chan->sgbuff, sizeof(pothos_zynq_dma_buff_t)) != 0) return -EACCES;

    return 0;
}

long pothos_zynq_dma_ioctl_free(pothos_zynq_dma_user_t *user)
{
    pothos_zynq_dma_chan_t *chan = user->chan;
    struct platform_device *pdev = user->engine->pdev;

    //are we already free?
    if (chan->allocs.buffs == NULL) return 0;

    //free dma buffers
    for (size_t i = 0; i < chan->allocs.num_buffs; i++)
    {
        if (chan->allocs.buffs[i].kaddr == NULL) continue; //alloc failed eariler
        dma_free_coherent(&pdev->dev, chan->allocs.buffs[i].bytes, chan->allocs.buffs[i].kaddr, chan->allocs.buffs[i].paddr);
    }

    //free the SG buffer
    dma_free_coherent(&pdev->dev, chan->sgbuff.bytes, chan->sgbuff.kaddr, chan->sgbuff.paddr);
    chan->sgtable = NULL;

    //free the dma buffer structures
    devm_kfree(&pdev->dev, chan->allocs.buffs);
    chan->allocs.num_buffs = 0;
    chan->allocs.buffs = NULL;

    return 0;
}
