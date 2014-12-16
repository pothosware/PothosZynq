// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "pothos_zynq_dma_module.h"
#include <linux/slab.h> //kmalloc, kfree
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/io.h> //ioremap
#include <linux/string.h> //strcmp

static struct file_operations pothos_zynq_dma_fops = {
    unlocked_ioctl: pothos_zynq_dma_ioctl,
    mmap: pothos_zynq_dma_mmap,
    open: pothos_zynq_dma_open,
    release: pothos_zynq_dma_release
};

/*!
 * Find the index in the tree in which this node appears
 * among other compatible nodes of the same type.
 */
static int node_to_index(struct device_node *node)
{
    struct device_node *n;
    int i = 0;
    for_each_compatible_node(n, NULL, "pothos,xlnx,axi-dma")
    {
        if (strcmp(of_node_full_name(n), of_node_full_name(node)) == 0) return i;
        i++;
    }
    return -1;
}

static int pothos_zynq_dma_probe(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;

    //determine device index
    int index = node_to_index(node);
    if (index < 0)
    {
        dev_err(&pdev->dev, "Error determining index for %s\n", of_node_full_name(node));
        return -EIO;
    }
    dev_info(&pdev->dev, "Loading %s as index %d\n", of_node_full_name(node), index);

    //format a device name from the index
    char device_name[1024];
    if (snprintf(device_name, sizeof(device_name), "pothos_zynq_dma%d", index) <= 0)
    {
        dev_err(&pdev->dev, "Failed to format a device name\n");
        return -EIO;
    }

    //inspect register entry
    struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res == NULL)
    {
        dev_err(&pdev->dev, "Error getting regs resource from devicetree.\nExample 'reg = <0x40400000 0x10000>;'\n");
        return -EIO;
    }
    dev_info(&pdev->dev, "Register start 0x%x\n", res->start);

    //inspect interrupts
    size_t irqno = 0;
    while(1)
    {
        unsigned int irq = irq_of_parse_and_map(node, irqno);
        if (irq == 0) break;
        else irqno++;
        dev_info(&pdev->dev, "Found IRQ %u\n", irq);
    }
    if (irqno == 0)
    {
        dev_err(&pdev->dev, "Error getting IRQ resource from devicetree.\nExample 'interrupts = <0 30 4>, <0 29 4>;'\n");
        return -EIO;
    }

    //allocate data structure for this device
    pothos_zynq_dma_device_t *data = devm_kzalloc(&pdev->dev, sizeof(pothos_zynq_dma_device_t), GFP_KERNEL);
    if (data == NULL) return -ENOMEM;
    atomic_long_set(&data->use_count, 0);
    data->pdev = pdev;
    dev_set_drvdata(&pdev->dev, data);

    //store registers into data
    data->regs_phys_addr = res->start;
    data->regs_phys_size = resource_size(res);

    //create character device
    if (alloc_chrdev_region(&data->dev_num, 0, 1, device_name) < 0)
    {
        return -1;
    }
    if ((data->cl = class_create(THIS_MODULE, device_name)) == NULL)
    {
        unregister_chrdev_region(data->dev_num, 1);
        return -1;
    }
    if (device_create(data->cl, NULL, data->dev_num, NULL, device_name) == NULL)
    {
        class_destroy(data->cl);
        unregister_chrdev_region(data->dev_num, 1);
        return -1;
    }
    cdev_init(&data->c_dev, &pothos_zynq_dma_fops);
    if (cdev_add(&data->c_dev, data->dev_num, 1) == -1)
    {
        device_destroy(data->cl, data->dev_num);
        class_destroy(data->cl);
        unregister_chrdev_region(data->dev_num, 1);
        return -1;
    }
    dev_info(&pdev->dev, "Created devfs entry at /dev/%s\n", device_name);

    return 0;
}

static int pothos_zynq_dma_remove(struct platform_device *pdev)
{
    pothos_zynq_dma_device_t *data = dev_get_drvdata(&pdev->dev);

    //remove charcter device
    cdev_del(&data->c_dev);
    device_destroy(data->cl, data->dev_num);
    class_destroy(data->cl);
    unregister_chrdev_region(data->dev_num, 1);

    //free the device data
    devm_kfree(&pdev->dev, data);
    dev_set_drvdata(&pdev->dev, NULL);

    return 0;
}

/***********************************************************************
 * register this platform driver into the system
 **********************************************************************/
static const struct of_device_id pothos_zynq_dma_of_match[] = {
    { .compatible = "pothos,xlnx,axi-dma", },
    {}
};
MODULE_DEVICE_TABLE(of, pothos_zynq_dma_of_match);

static struct platform_driver pothos_zynq_dma_driver = {
    .driver = {
        .name = "pothos-xilinx-dma",
        .of_match_table = pothos_zynq_dma_of_match,
    },
    .probe = pothos_zynq_dma_probe,
    .remove = pothos_zynq_dma_remove,
};
module_platform_driver(pothos_zynq_dma_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Josh Blum");
MODULE_DESCRIPTION("Pothos AXI Stream DMA");
