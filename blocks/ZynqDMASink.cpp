// Copyright (c) 2013-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ZynqDMASupport.hpp"
#include <iostream>

/***********************************************************************
 * |PothosDoc Zynq DMA Sink
 *
 * Test block for sending DMA buffers into the PL.
 *
 * |category /Zynq
 * |category /Sinks
 * |keywords zynq dma
 *
 * |param index[Engine Index] The index of an AXI DMA on the system
 * |default 0
 *
 * |factory /zynq/dma_sink(index)
 **********************************************************************/
class ZyncDMASink : public Pothos::Block
{
public:
    static Block *make(const size_t index)
    {
        return new ZyncDMASink(index);
    }

    ZyncDMASink(const size_t index):
        _engine(std::shared_ptr<pzdud_t>(pzdud_create(index), &pzdud_destroy))
    {
        if (not _engine) throw Pothos::Exception("ZyncDMASink::pzdud_create()");
        this->setupInput(0, "", "ZyncDMASink"+std::to_string(index));
    }

    Pothos::BufferManager::Sptr getInputBufferManager(const std::string &, const std::string &domain)
    {
        if (domain.empty())
        {
            return makeZynqDMABufferManager(_engine, PZDUD_MM2S);
        }
        throw Pothos::PortDomainError();
    }

    void work(void)
    {
        auto inPort = this->input(0);

        //check if a buffer is available
        if (inPort->elements() == 0) return;

        //wait for completion on the head buffer
        const long timeout_us = this->workInfo().maxTimeoutNs/1000;
        const int ret = pzdud_wait(_engine.get(), PZDUD_MM2S, timeout_us);
        if (ret == PZDUD_ERROR_TIMEOUT)
        {
            //got a timeout, yield so we can get called again
            return this->yield();
        }

        //some other kind of error from wait occurred:
        else if (ret != PZDUD_OK)
        {
            throw Pothos::Exception("ZyncDMASink::pzdud_wait()", std::to_string(ret));
        }

        //acquire the head buffer and release its handle
        size_t length = 0; //length not used for MM2S
        const int handle = pzdud_acquire(_engine.get(), PZDUD_MM2S, &length);
        if (handle < 0) throw Pothos::Exception("ZyncDMASink::pzdud_acquire()", std::to_string(handle));
        //the handle could be out of order, so we dont check its value
        //we assume that out of order buffers means that we waited on
        //more xfers, not less xfers, including this handle's xfers

        //consume the buffer from the input port
        inPort->consume(inPort->elements());
    }

private:
    std::shared_ptr<pzdud_t> _engine;
};

static Pothos::BlockRegistry registerZyncDMASink(
    "/zynq/dma_sink", &ZyncDMASink::make);
