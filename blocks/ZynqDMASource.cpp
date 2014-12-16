// Copyright (c) 2013-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ZynqDMASupport.hpp"
#include <iostream>

/***********************************************************************
 * |PothosDoc Zynq DMA Source
 *
 * Test block for receiving DMA buffers from the PL.
 *
 * |category /Zynq
 * |category /Sources
 * |keywords zynq dma
 *
 * |param index[Engine Index] The index of an AXI DMA on the system
 * |default 0
 *
 * |factory /zynq/dma_source(index)
 **********************************************************************/
class ZyncDMASource : public Pothos::Block
{
public:
    static Block *make(const size_t index)
    {
        return new ZyncDMASource(index);
    }

    ZyncDMASource(const size_t index):
        _engine(std::shared_ptr<pzdud_t>(pzdud_create(index), &pzdud_destroy))
    {
        if (not _engine) throw Pothos::Exception("ZyncDMASource::pzdud_create()");
        this->setupOutput(0, "", "ZyncDMASource"+std::to_string(index));
    }

    Pothos::BufferManager::Sptr getOutputBufferManager(const std::string &, const std::string &domain)
    {
        if (domain.empty())
        {
            return makeZynqDMABufferManager(_engine, PZDUD_S2MM);
        }
        throw Pothos::PortDomainError();
    }

    void work(void)
    {
        auto outPort = this->output(0);

        //check if a buffer is available
        if (outPort->elements() == 0) return;

        //wait for completion on the head buffer
        const long timeout_us = this->workInfo().maxTimeoutNs/1000;
        const int ret = pzdud_wait(_engine.get(), PZDUD_S2MM, timeout_us);
        if (ret == PZDUD_ERROR_TIMEOUT)
        {
            //got a timeout, yield so we can get called again
            return this->yield();
        }

        //some other kind of error from wait occurred:
        else if (ret != PZDUD_OK)
        {
            throw Pothos::Exception("ZyncDMASource::pzdud_wait()", std::to_string(ret));
        }

        //acquire the head buffer and release its handle
        size_t length = 0;
        const int handle = pzdud_acquire(_engine.get(), PZDUD_S2MM, &length);
        if (handle < 0) throw Pothos::Exception("ZyncDMASource::pzdud_acquire()", std::to_string(handle));
        if (size_t(handle) != outPort->buffer().getManagedBuffer().getSlabIndex())
        {
            throw Pothos::Exception("ZyncDMASource::pzdud_acquire()", "out of order handle");
        }

        //produce the buffer to the output port
        outPort->produce(length);
    }

private:
    std::shared_ptr<pzdud_t> _engine;
};

static Pothos::BlockRegistry registerZyncDMASource(
    "/zynq/dma_source", &ZyncDMASource::make);
