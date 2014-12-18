// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "pothos_zynq_dma_module.h"
#include <linux/uaccess.h> //copy_to/from_user
#include <linux/wait.h> //wait_queue_head_t
#include <linux/sched.h> //interruptible
#include <linux/io.h> //iowrite32

irqreturn_t pothos_zynq_dma_irq_handler(int irq, void *data)
{
    pothos_zynq_dma_chan_t *chan = (pothos_zynq_dma_chan_t *)data;
    chan->irq_count++;

    //ack the interrupts
    iowrite32(XILINX_DMA_XR_IRQ_ALL_MASK, chan->register_stat);

    //wake up any contexts which are blocking on the wait queue
    wake_up_interruptible(&chan->irq_wait);

    return IRQ_HANDLED;
}

long pothos_zynq_dma_ioctl_wait(pothos_zynq_dma_user_t *user, const pothos_zynq_dma_wait_t *user_config)
{
    //convert the args into kernel memory
    pothos_zynq_dma_wait_t wait_args;
    if (copy_from_user(&wait_args, user_config, sizeof(pothos_zynq_dma_wait_t)) != 0) return -EACCES;

    //check the sentinel
    if (wait_args.sentinel != POTHOS_ZYNQ_DMA_SENTINEL) return -EINVAL;

    //check that interrupts are configured
    if (user->chan->irq_number == 0 || user->chan->irq_registered != 0) return -ENODEV;

    //check that the SG index is in range
    if (wait_args.sgindex >= user->chan->allocs.num_buffs) return -ECHRNG;

    //check that the SG table is set
    if (user->chan->sgtable == NULL) return -EADDRNOTAVAIL;

    //offset to the scatter/gather entry (last buff is sg)
    xilinx_dma_desc_t *desc = user->chan->sgtable + wait_args.sgindex;

    //wait on the condition
    const unsigned long timeout = usecs_to_jiffies(wait_args.timeout_us);
    wait_event_interruptible_timeout(user->chan->irq_wait, ((desc->status & (1 << 31)) != 0), timeout);
    return 0;
}
