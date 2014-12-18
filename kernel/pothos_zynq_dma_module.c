// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "pothos_zynq_dma_module.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/slab.h> //kalloc
#include <linux/io.h> //ioremap

/***********************************************************************
 * Module data structures
 **********************************************************************/
static struct file_operations pothos_zynq_dma_fops = {
    unlocked_ioctl: pothos_zynq_dma_ioctl,
    mmap: pothos_zynq_dma_mmap,
    open: pothos_zynq_dma_open,
    release: pothos_zynq_dma_release
};

static pothos_zynq_dma_module_t module_data;

/***********************************************************************
 * Initialize channel data
 **********************************************************************/
static void pothos_zynq_dma_chan_clear(pothos_zynq_dma_chan_t *chan)
{
    chan->allocs.num_buffs = 0;
    chan->allocs.buffs = NULL;
    chan->sgbuff.paddr = 0;
    chan->sgbuff.kaddr = NULL;
    chan->sgbuff.uaddr = NULL;
    chan->sgtable = NULL;
    chan->register_ctrl = NULL;
    chan->register_stat = NULL;
    chan->irq_number = 0;
    init_waitqueue_head(&chan->irq_wait);
    chan->irq_count = 0;
    chan->irq_registered = 0;
    chan->claimed = 0;
}

/***********************************************************************
 * IRQ registration helpers
 **********************************************************************/
static void pothos_zynq_dma_chan_register_irq(struct platform_device *pdev, pothos_zynq_dma_chan_t *chan)
{
    if (chan->irq_number == 0) return;
    chan->irq_registered = devm_request_irq(&pdev->dev, chan->irq_number, pothos_zynq_dma_irq_handler, IRQF_SHARED, "xilinx-dma-controller", chan);
}

static void pothos_zynq_dma_chan_unregister_irq(struct platform_device *pdev, pothos_zynq_dma_chan_t *chan)
{
    if (chan->irq_number == 0) return;
    if (chan->irq_registered != 0) return;
    devm_free_irq(&pdev->dev, chan->irq_number, chan);
}

/***********************************************************************
 * Per-engine initializer
 **********************************************************************/
static int pothos_zynq_dma_engine_init(pothos_zynq_dma_engine_t *engine, struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;

    //init engine data structures
    engine->pdev = pdev;
    engine->regs_phys_addr = 0;
    engine->regs_phys_size = 0;
    engine->regs_virt_addr = NULL;

    //extract the register space
    struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res == NULL)
    {
        dev_err(&pdev->dev, "Error getting regs resource from devicetree.'\n");
        dev_err(&pdev->dev, "Example 'reg = <0x40400000 0x10000>;'\n");
        return -1;
    }
    dev_info(&pdev->dev, "Registers at 0x%x\n", engine->regs_phys_addr);

    //map the register space
    engine->regs_phys_addr = res->start;
    engine->regs_phys_size = resource_size(res)/2; //we choose to map lower half
    engine->regs_virt_addr = ioremap_nocache(engine->regs_phys_addr, engine->regs_phys_size);
    if (engine->regs_virt_addr == NULL)
    {
        dev_info(&pdev->dev, "Error mapping register resource\n");
        return -1;
    }

    //clear the channels
    pothos_zynq_dma_chan_clear(&engine->mm2s_chan);
    pothos_zynq_dma_chan_clear(&engine->s2mm_chan);

    //load register offsets into channels
    engine->mm2s_chan.register_ctrl = (void *)((size_t)engine->regs_virt_addr + XILINX_DMA_MM2S_DMACR_OFFSET);
    engine->mm2s_chan.register_stat = (void *)((size_t)engine->regs_virt_addr + XILINX_DMA_MM2S_DMASR_OFFSET);
    engine->s2mm_chan.register_ctrl = (void *)((size_t)engine->regs_virt_addr + XILINX_DMA_S2MM_DMACR_OFFSET);
    engine->s2mm_chan.register_stat = (void *)((size_t)engine->regs_virt_addr + XILINX_DMA_S2MM_DMASR_OFFSET);

    //determine interrupt numbers
    engine->mm2s_chan.irq_number = irq_of_parse_and_map(node, 0);
    dev_info(&pdev->dev, "MM2S IRQ = %d\n", engine->mm2s_chan.irq_number);
    engine->s2mm_chan.irq_number = irq_of_parse_and_map(node, 1);
    dev_info(&pdev->dev, "S2MM IRQ = %d\n", engine->s2mm_chan.irq_number);
    if (engine->mm2s_chan.irq_number == 0 || engine->s2mm_chan.irq_number == 0)
    {
        dev_err(&pdev->dev, "Error getting IRQ resources from devicetree.\n");
        dev_err(&pdev->dev, "Example 'interrupts = <0 30 4>, <0 29 4>;'\n");
        return -1;
    }

    //register interrupt handlers
    pothos_zynq_dma_chan_register_irq(pdev, &engine->mm2s_chan);
    pothos_zynq_dma_chan_register_irq(pdev, &engine->s2mm_chan);

    return 0;
}

/***********************************************************************
 * Per-engine cleanup
 **********************************************************************/
static void pothos_zynq_dma_engine_exit(pothos_zynq_dma_engine_t *engine)
{
    struct platform_device *pdev = engine->pdev;

    //unregister interrupt handles
    dev_info(&pdev->dev, "MM2S IRQ total = %llu\n", engine->mm2s_chan.irq_count);
    dev_info(&pdev->dev, "S2MM IRQ total = %llu\n", engine->s2mm_chan.irq_count);
    pothos_zynq_dma_chan_unregister_irq(pdev, &engine->mm2s_chan);
    pothos_zynq_dma_chan_unregister_irq(pdev, &engine->s2mm_chan);

    //unmap registers
    if (engine->regs_virt_addr != NULL) iounmap(engine->regs_virt_addr);
}

/***********************************************************************
 * Module entry point
 **********************************************************************/
static int pothos_zynq_dma_module_init(void)
{
    //initialize module data
    module_data.engines = NULL;
    module_data.num_engines = 0;

    //locate the platform device
    struct device_node *node = NULL;
    for_each_compatible_node(node, NULL, "pothos,xlnx,axi-dma")
    {
        struct platform_device *pdev = of_find_device_by_node(node);
        if (pdev == NULL) continue;
        module_data.num_engines++;
        module_data.engines = krealloc(module_data.engines, sizeof(pothos_zynq_dma_engine_t)*module_data.num_engines, GFP_KERNEL);
        if (pothos_zynq_dma_engine_init(module_data.engines+module_data.num_engines-1, pdev) != 0) return -1;
    }

    //register the character device
    if (alloc_chrdev_region(&module_data.dev_num, 0, 1, MODULE_NAME) < 0)
    {
        return -1;
    }
    if ((module_data.cl = class_create(THIS_MODULE, MODULE_NAME)) == NULL)
    {
        unregister_chrdev_region(module_data.dev_num, 1);
        return -1;
    }
    if (device_create(module_data.cl, NULL, module_data.dev_num, NULL, MODULE_NAME) == NULL)
    {
        class_destroy(module_data.cl);
        unregister_chrdev_region(module_data.dev_num, 1);
        return -1;
    }
    cdev_init(&module_data.c_dev, &pothos_zynq_dma_fops);
    if (cdev_add(&module_data.c_dev, module_data.dev_num, 1) == -1)
    {
        device_destroy(module_data.cl, module_data.dev_num);
        class_destroy(module_data.cl);
        unregister_chrdev_region(module_data.dev_num, 1);
        return -1;
    }
    return 0;
}

/***********************************************************************
 * Module exit point
 **********************************************************************/
static void pothos_zynq_dma_module_exit(void)
{
    //remove the character device
    cdev_del(&module_data.c_dev);
    device_destroy(module_data.cl, module_data.dev_num);
    class_destroy(module_data.cl);
    unregister_chrdev_region(module_data.dev_num, 1);

    //cleanup each dma engine
    for (size_t i = 0; i < module_data.num_engines; i++)
    {
        pothos_zynq_dma_engine_exit(module_data.engines + i);
    }

    kfree(module_data.engines);
}

/***********************************************************************
 * Module registration
 **********************************************************************/
MODULE_LICENSE("Dual BSD/GPL");
module_init(pothos_zynq_dma_module_init);
module_exit(pothos_zynq_dma_module_exit);
