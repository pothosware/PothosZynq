// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

/***********************************************************************
 * AXI DMA v7.1 userspace driver for Scatter/Gather mode
 **********************************************************************/

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

//! Return error codes
#define PZDUD_OK 0
#define PZDUD_ERROR_NOSG -1 //!< scatter/gather feature not detected
#define PZDUD_ERROR_TIMEOUT -2 //!< wait timeout or loop timeout
#define PZDUD_ERROR_ALLOC -5 //!< error allocating DMA buffers
#define PZDUD_ERROR_CLAIMED -6 //!< all buffers claimed by the user
#define PZDUD_ERROR_COMPLETE -7 //!< no completed buffer transactions

//! Direction constants to specify memory to/from stream
typedef enum pzdud_dir
{
    PZDUD_S2MM,
    PZDUD_MM2S,
} pzdud_dir_t;

//! opaque struct for dma driver instance
struct pzdud;
typedef struct pzdud pzdud_t;

/*!
 * Create a new user DMA instance.
 * The instance represents a single DMA channel
 * given the engine index and the channel direction.
 * \param engine_no the index of an AXI DMA in the device tree
 * \param direction the direction to/from stream
 * \return the user dma instance structure or NULL on error
 */
static inline pzdud_t *pzdud_create(const size_t engine_no, const pzdud_dir_t direction);

/*!
 * Destroy a user DMA instance.
 * \param self the user dma instance structure
 * \return the error code or 0 for success
 */
static inline int pzdud_destroy(pzdud_t *self);

/*!
 * Reset the DMA engine.
 * Although S2MM and MM2s channels are independent,
 * this call resets the entire engine, both channels,
 * regardless of the channel direction for this instance.
 * Use with caution as this could halt another instance.
 * \param self the user dma instance structure
 * \return the error code or 0 for success
 */
static inline int pzdud_reset(pzdud_t *self);

/*!
 * Allocate buffers and setup the scatter/gather table.
 * Call pzdud_alloc before initializing the engine.
 * \param self the user dma instance structure
 * \param num_buffs the number of buffers in the table
 * \param buff_size the size of the buffers in bytes
 * \return the error code or 0 for success
 */
static inline int pzdud_alloc(pzdud_t *self, const size_t num_buffs, const size_t buff_size);

/*!
 * Free buffers allocated by pzdud_alloc.
 * Only call pzdud_free when the engine is halted.
 * \param self the user dma instance structure
 * \return the error code or 0 for success
 */
static inline int pzdud_free(pzdud_t *self);

/*!
 * Get the address of a buffer for the given handle.
 * This is a virtual userspace address that can be read/written.
 *
 * The handles range from 0 to num_buffs - 1.
 * This is the same handle returned by pzdud_acquire().
 * The addresses for a given handle are available after pzdud_alloc(),
 * and will not change value unless free() and alloc() are called again.
 *
 * \param self the user dma instance structure
 * \param handle the handle value/buffer index
 * \return the address of the DMA buffer (NULL if index out of range)
 */
static inline void *pzdud_addr(pzdud_t *self, size_t handle);

/*!
 * Initialize the DMA engine for streaming.
 * The engine will be ready to receive streams.
 *
 * In a typical use case, release is true, meaning the
 * fist user call after init should be wait or acquire.
 *
 * \param self the user dma instance structure
 * \param release true to initialize buffers with engine ownership
 * \return the error code or 0 for success
 */
static inline int pzdud_init(pzdud_t *self, const bool release);

/*!
 * Halt/stop all ongoing transfer activity.
 * \param self the user dma instance structure
 * \return the error code or 0 for success
 */
static inline int pzdud_halt(pzdud_t *self);

/*!
 * Wait for a DMA buffer to become available.
 * \param self the user dma instance structure
 * \param timeout_us the timeout in microseconds
 *
 * \return the error code for timeout or 0 for success
 */
static inline int pzdud_wait(pzdud_t *self, const long timeout_us);

/*!
 * Acquire a DMA buffer from the engine.
 * The length value has the number of bytes filled by the transfer.
 * Return PZDUD_ERROR_COMPLETE when there are no completed transactions.
 * Return PZDUD_ERROR_CLAIMED when the user has acquired all buffers.
 * Otherwise return a handle that can be used to release the buffer.
 *
 * \param self the user dma instance structure
 * \param [out] length the buffer length in bytes
 * \return the handle or negative error code
 */
static inline int pzdud_acquire(pzdud_t *self, size_t *length);

/*!
 * Release a DMA buffer back the engine.
 * Returns immediately, no errors.
 * \param self the user dma instance structure
 * \param length the length in bytes to submit (MM2S only)
 * \param handle the handle value from the acquire result
 */
static inline void pzdud_release(pzdud_t *self, size_t handle, size_t length);

/*!
 * Write a user application field to the SG table.
 * These values will be output in the control stream.
 * This call only applies to the MM2S direction.
 * \param self the user dma instance structure
 * \param handle the handle for a specific SG entry
 * \param which which application field 0 to 4
 * \param value the value for the user field
 */
static inline void pzdud_set_app_field(pzdud_t *self, size_t handle, size_t which, const uint32_t value);

/*!
 * Read a user application field from the SG table.
 * These values will be input from the status stream.
 * This call only applies to the S2MM direction.
 * \param self the user dma instance structure
 * \param handle the handle for a specific SG entry
 * \param which which application field 0 to 4
 * \return the value for the user field
 */
static inline uint32_t pzdud_get_app_field(pzdud_t *self, size_t handle, size_t which);

/***********************************************************************
 * implementation
 **********************************************************************/
#include "pothos_zynq_dma_common.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h> //mmap
#include <unistd.h> //close
#include <stdlib.h>
#include <string.h>

/***********************************************************************
 * Definition for instance data
 **********************************************************************/
struct pzdud
{
    int fd; //!< file descriptor for device node
    void *regs; //!< mapped register space

    //! configuration params
    size_t engine_no;
    pzdud_dir_t direction;

    //! mapped registers
    void *ctrl_reg;
    void *stat_reg;
    void *head_reg;
    void *tail_reg;

    //! buffer allocation
    size_t num_buffs;
    size_t buff_size;
    pothos_zynq_dma_alloc_t allocs;

    //! buffer tracking
    size_t head_index;
    size_t tail_index;
    size_t num_acquired;

    xilinx_dma_desc_t *sgtable;
};

/***********************************************************************
 * Helper functions
 **********************************************************************/
static inline void __pzdud_write32(void *addr, uint32_t val)
{
    volatile uint32_t *p = (volatile uint32_t *)(addr);
    *p = val;
}

static inline uint32_t __pzdud_read32(void *addr)
{
    volatile uint32_t *p = (volatile uint32_t *)(addr);
    return *p;
}

static inline size_t __pzdud_virt_to_phys(void *virt, const pothos_zynq_dma_buff_t *buff)
{
    size_t offset = (size_t)virt - (size_t)buff->uaddr;
    return offset + buff->paddr;
}

/***********************************************************************
 * create/destroy implementation
 **********************************************************************/
static inline pzdud_t *pzdud_create(const size_t engine_no, const pzdud_dir_t direction)
{
    //open the device
    int fd = open("/dev/pothos_zynq_dma", O_RDWR | O_SYNC);
    if (fd <= 0)
    {
        perror("pzdud_create::open()");
        return NULL;
    }

    //associate the channel
    pothos_zynq_dma_setup_t setup_args;
    setup_args.sentinel = POTHOS_ZYNQ_DMA_SENTINEL;
    setup_args.engine_no = engine_no;
    setup_args.direction = (direction == PZDUD_S2MM)?POTHOS_ZYNQ_DMA_S2MM:POTHOS_ZYNQ_DMA_MM2S;
    if (ioctl(fd, POTHOS_ZYNQ_DMA_SETUP, (void *)&setup_args) != 0)
    {
        perror("pzdud_create::ioctl(setup)");
        return NULL;
    }

    //map the register space
    void *regs = mmap(NULL, POTHOS_ZYNQ_DMA_REGS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, POTHOS_ZYNQ_DMA_REGS_OFF);
    if (regs == MAP_FAILED)
    {
        perror("pzdud_create::mmap(regs)");
        return NULL;
    }

    //initialize the object structure
    pzdud_t *self = (pzdud_t *)calloc(1, sizeof(pzdud_t));
    self->fd = fd;
    self->regs = regs;

    self->engine_no = engine_no;
    self->direction = direction;

    if (direction == PZDUD_S2MM)
    {
        self->ctrl_reg = ((char *)regs) + XILINX_DMA_S2MM_DMACR_OFFSET;
        self->stat_reg = ((char *)regs) + XILINX_DMA_S2MM_DMASR_OFFSET;
        self->head_reg = ((char *)regs) + XILINX_DMA_S2MM_CURDESC_OFFSET;
        self->tail_reg = ((char *)regs) + XILINX_DMA_S2MM_TAILDESC_OFFSET;
    }

    if (direction == PZDUD_MM2S)
    {
        self->ctrl_reg = ((char *)regs) + XILINX_DMA_MM2S_DMACR_OFFSET;
        self->stat_reg = ((char *)regs) + XILINX_DMA_MM2S_DMASR_OFFSET;
        self->head_reg = ((char *)regs) + XILINX_DMA_MM2S_CURDESC_OFFSET;
        self->tail_reg = ((char *)regs) + XILINX_DMA_MM2S_TAILDESC_OFFSET;
    }

    return self;
}

static inline int pzdud_destroy(pzdud_t *self)
{
    munmap(self->regs, POTHOS_ZYNQ_DMA_REGS_SIZE);
    close(self->fd);
    free(self);
    return PZDUD_OK;
}

/***********************************************************************
 * reset implementation
 **********************************************************************/
static inline int pzdud_reset(pzdud_t *self)
{
    //perform a soft reset and wait for done
    __pzdud_write32(self->ctrl_reg, __pzdud_read32(self->ctrl_reg) | XILINX_DMA_CR_RESET_MASK);
    int loop = XILINX_DMA_RESET_LOOP;
    while ((__pzdud_read32(self->ctrl_reg) & XILINX_DMA_CR_RESET_MASK) != 0)
    {
        if (--loop == 0) return PZDUD_ERROR_TIMEOUT;
    }

    return PZDUD_OK;
}

/***********************************************************************
 * allocation implementation
 **********************************************************************/
static inline int pzdud_alloc(pzdud_t *self, const size_t num_buffs, const size_t buff_size)
{
    pothos_zynq_dma_alloc_t *allocs = &self->allocs;
    memset(allocs, 0, sizeof(pothos_zynq_dma_alloc_t));
    self->num_buffs = num_buffs;
    self->buff_size = buff_size;

    //load up the allocation request
    allocs->sentinel = POTHOS_ZYNQ_DMA_SENTINEL;
    allocs->num_buffs = num_buffs;
    allocs->buffs = (pothos_zynq_dma_buff_t *)calloc(num_buffs, sizeof(pothos_zynq_dma_buff_t));
    for (size_t i = 0; i < num_buffs; i++)
    {
        allocs->buffs[i].bytes = buff_size;
    }

    //perform the allocation ioctl
    int ret = ioctl(self->fd, POTHOS_ZYNQ_DMA_ALLOC, (void *)allocs);
    if (ret != 0)
    {
        perror("pzdud_alloc::ioctl(alloc)");
        return PZDUD_ERROR_ALLOC;
    }

    //check the results and mmap
    for (size_t i = 0; i < num_buffs; i++)
    {
        pothos_zynq_dma_buff_t *buff = allocs->buffs + i;
        if (buff->paddr == 0 || buff->kaddr == NULL) goto fail;
        buff->uaddr = mmap(NULL, buff->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, self->fd, buff->paddr);
        if (buff->uaddr == MAP_FAILED) goto fail;
    }

    //the last buffer is used for the sg table
    {
        pothos_zynq_dma_buff_t *buff = &allocs->sgbuff;
        if (buff->paddr == 0 || buff->kaddr == NULL) goto fail;
        buff->uaddr = mmap(NULL, buff->bytes, PROT_READ | PROT_WRITE, MAP_SHARED, self->fd, buff->paddr);
        if (buff->uaddr == MAP_FAILED) goto fail;
        self->sgtable = (xilinx_dma_desc_t *)buff->uaddr;
    }

    return PZDUD_OK;

    fail:
        ioctl(self->fd, POTHOS_ZYNQ_DMA_FREE);
        return PZDUD_ERROR_ALLOC;
}

static inline int pzdud_free(pzdud_t *self)
{
    pothos_zynq_dma_alloc_t *allocs = &self->allocs;

    //unmap all the buffers
    for (size_t i = 0; i < allocs->num_buffs; i++)
    {
        pothos_zynq_dma_buff_t *buff = allocs->buffs + i;
        if (buff->uaddr != MAP_FAILED) munmap(buff->uaddr, buff->bytes);
    }
    {
        pothos_zynq_dma_buff_t *buff = &allocs->sgbuff;
        if (buff->uaddr != MAP_FAILED) munmap(buff->uaddr, buff->bytes);
    }

    //free all the buffers
    int ret = ioctl(self->fd, POTHOS_ZYNQ_DMA_FREE);
    if (ret != 0)
    {
        perror("pzdud_free::ioctl(free)");
        return PZDUD_ERROR_ALLOC;
    }

    //free the container
    free(allocs->buffs);
    allocs->buffs = NULL;
    allocs->num_buffs = 0;

    return PZDUD_OK;
}

static inline void *pzdud_addr(pzdud_t *self, size_t handle)
{
    if (handle >= self->num_buffs) return NULL;

    return self->allocs.buffs[handle].uaddr;
}

/***********************************************************************
 * init/halt implementation
 **********************************************************************/
static inline int pzdud_init(pzdud_t *self, const bool release)
{
    //check for scatter/gather support
    if ((__pzdud_read32(self->stat_reg) & 0x8) == 0) return PZDUD_ERROR_NOSG;

    //load the scatter gather table
    for (size_t i = 0; i < self->num_buffs; i++)
    {
        xilinx_dma_desc_t *desc = self->sgtable + i;
        size_t next_index = (i+1) % self->num_buffs;
        xilinx_dma_desc_t *next = self->sgtable + next_index;
        desc->next_desc = __pzdud_virt_to_phys(next, &self->allocs.sgbuff);
        desc->buf_addr = self->allocs.buffs[i].paddr;
        desc->control = 0;
        desc->status = (1 << 31); //mark completed (ownership to caller)
    }

    //initialize buffer tracking
    self->head_index = 0;
    self->tail_index = 0;
    self->num_acquired = self->num_buffs;

    //load desc pointers
    xilinx_dma_desc_t *head = self->sgtable + self->head_index;
    __pzdud_write32(self->head_reg, __pzdud_virt_to_phys(head, &self->allocs.sgbuff));
    xilinx_dma_desc_t *tail = self->sgtable + self->tail_index;
    __pzdud_write32(self->tail_reg, __pzdud_virt_to_phys(tail, &self->allocs.sgbuff));

    //start the engine
    __pzdud_write32(self->ctrl_reg, __pzdud_read32(self->ctrl_reg) | XILINX_DMA_CR_RUNSTOP_MASK);

    //enable interrupt on complete
    __pzdud_write32(self->ctrl_reg, __pzdud_read32(self->ctrl_reg) | XILINX_DMA_XR_IRQ_IOC_MASK);

    //release all the buffers into the engine
    if (release) for (size_t i = 0; i < self->num_buffs; i++)
    {
        if (self->direction == PZDUD_S2MM) pzdud_release(self, i, 0);
        else self->num_acquired--;
    }

    return PZDUD_OK;
}

static inline int pzdud_halt(pzdud_t *self)
{
    //perform a halt and wait for done
    __pzdud_write32(self->ctrl_reg, __pzdud_read32(self->ctrl_reg) | ~XILINX_DMA_CR_RUNSTOP_MASK);
    int loop = XILINX_DMA_HALT_LOOP;
    while ((__pzdud_read32(self->ctrl_reg) & XILINX_DMA_CR_RUNSTOP_MASK) != 0)
    {
        if (--loop == 0) return PZDUD_ERROR_TIMEOUT;
    }

    return PZDUD_OK;
}

/***********************************************************************
 * acquire/release implementation
 **********************************************************************/
static inline int pzdud_wait(pzdud_t *self, const long timeout_us)
{
    if (__sync_fetch_and_add(&self->num_acquired, 0) == self->num_buffs) return PZDUD_ERROR_CLAIMED;

    xilinx_dma_desc_t *desc = self->sgtable+self->head_index;

    //initial check without blocking
    if ((desc->status & (1 << 31)) != 0) return PZDUD_OK;

    //check completion status of the buffer with timeout
    if (timeout_us > 0)
    {
        pothos_zynq_dma_wait_t wait_args;
        wait_args.sentinel = POTHOS_ZYNQ_DMA_SENTINEL;
        wait_args.timeout_us = timeout_us;
        wait_args.sgindex = self->head_index;
        int ret = ioctl(self->fd, POTHOS_ZYNQ_DMA_WAIT, (void *)&wait_args);
        if (ret != 0)
        {
            perror("pzdud_free::ioctl(wait)");
            return PZDUD_ERROR_TIMEOUT;
        }
    }

    //check the condition for the last time
    if ((desc->status & (1 << 31)) != 0) return PZDUD_OK;
    return PZDUD_ERROR_TIMEOUT;
}

static inline int pzdud_acquire(pzdud_t *self, size_t *length)
{
    if (__sync_fetch_and_add(&self->num_acquired, 0) == self->num_buffs) return PZDUD_ERROR_CLAIMED;

    xilinx_dma_desc_t *desc = self->sgtable+self->head_index;

    //check completion status of the buffer
    if ((desc->status & (1 << 31)) == 0) return PZDUD_ERROR_COMPLETE;

    //fill in the buffer structure
    int handle = self->head_index;
    *length = (self->direction == PZDUD_S2MM)?(desc->status & 0x7fffff):(self->buff_size);

    //increment to next
    self->head_index = (self->head_index + 1) % self->num_buffs;
    __sync_fetch_and_add(&self->num_acquired, 1);

    return handle;
}

static inline void pzdud_release(pzdud_t *self, size_t handle, size_t length)
{
    uint32_t ctrl_word = (self->direction == PZDUD_S2MM)?(self->buff_size):(length | XILINX_DMA_BD_SOP | XILINX_DMA_BD_EOP);

    xilinx_dma_desc_t *desc = self->sgtable+handle;

    desc->control = ctrl_word; //new control flags
    desc->status = 0; //clear status

    //determine the new tail (buffers may not be released in order)
    do
    {
        xilinx_dma_desc_t *tail = self->sgtable + self->tail_index;
        if (tail->status != 0) break;
        __pzdud_write32(self->tail_reg, __pzdud_virt_to_phys(tail, &self->allocs.sgbuff));
        self->tail_index = (self->tail_index + 1) % self->num_buffs;
    }
    while (__sync_sub_and_fetch(&self->num_acquired, 1) != 0);
}

/***********************************************************************
 * user field access
 **********************************************************************/
static inline void pzdud_set_app_field(pzdud_t *self, size_t handle, size_t which, const uint32_t value)
{
    uint32_t *addr = &(self->sgtable[handle].app_0);
    *(addr + which) = value;
}

static inline uint32_t pzdud_get_app_field(pzdud_t *self, size_t handle, size_t which)
{
    const uint32_t *addr = &(self->sgtable[handle].app_0);
    return *(addr + which);
}
