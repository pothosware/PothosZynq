// Copyright (c) 2013-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "ZynqDMASupport.hpp"
#include <Pothos/Util/OrderedQueue.hpp>
#include <memory>
#include <iostream>

template <pzdud_dir_t dir>
class ZynqDMABufferManager :
    public Pothos::BufferManager,
    public std::enable_shared_from_this<ZynqDMABufferManager<dir>>
{
public:
    ZynqDMABufferManager(std::shared_ptr<pzdud_t> engine):
        _engine(engine)
    {
        return;
    }

    ~ZynqDMABufferManager(void)
    {
        if (this->isInitialized())
        {
            pzdud_halt(_engine.get(), dir);
            pzdud_free(_engine.get(), dir);
        }
    }

    void init(const Pothos::BufferManagerArgs &args)
    {
        _readyBuffs = Pothos::Util::OrderedQueue<Pothos::ManagedBuffer>(args.numBuffers);

        int ret = pzdud_alloc(_engine.get(), dir, args.numBuffers, args.bufferSize);
        if (ret != PZDUD_OK) throw Pothos::Exception("ZynqBufferManager::pzdud_alloc()", std::to_string(ret));

        ret = pzdud_init(_engine.get(), dir, false/*no initial release*/);
        if (ret != PZDUD_OK) throw Pothos::Exception("ZynqBufferManager::pzdud_init()", std::to_string(ret));

        //this will flag the manager as initialized after the allocation above
        Pothos::BufferManager::init(args);

        //create all the buffer containers...
        for (size_t handle = 0; handle < args.numBuffers; handle++)
        {
            auto container = std::make_shared<int>(0);
            void *addr = pzdud_addr(_engine.get(), dir, handle);
            auto sharedBuff = Pothos::SharedBuffer(size_t(addr), args.bufferSize, container);
            Pothos::ManagedBuffer buffer;
            buffer.reset(this->shared_from_this(), sharedBuff, handle);
        }
    }

    bool empty(void) const
    {
        return _readyBuffs.empty();
    }

    void pop(const size_t numBytes)
    {
        //boiler-plate to pop from the queue and set the front buffer
        assert(not _readyBuffs.empty());
        auto buff = _readyBuffs.front();
        _readyBuffs.pop();

        //prepare the next buffer in the queue
        if (_readyBuffs.empty()) this->setFrontBuffer(Pothos::BufferChunk::null());
        else this->setFrontBuffer(_readyBuffs.front());

        //pop == release in the dma to stream direction
        //this manager in an output port upstream of dma sink
        if (dir == PZDUD_MM2S)
        {
            pzdud_release(_engine.get(), dir, buff.getSlabIndex(), numBytes);
        }
    }

    void push(const Pothos::ManagedBuffer &buff)
    {
        assert(buff.getSlabIndex() < _readyBuffs.capacity());
        _readyBuffs.push(buff, buff.getSlabIndex());

        //prepare the next buffer in the queue
        if (not _readyBuffs.empty()) this->setFrontBuffer(_readyBuffs.front());

        //push == release in the stream to DMA direction
        //this manager in the output port on the dma source
        if (dir == PZDUD_S2MM)
        {
            pzdud_release(_engine.get(), dir, buff.getSlabIndex(), 0/*unused*/);
        }
    }

private:
    Pothos::Util::OrderedQueue<Pothos::ManagedBuffer> _readyBuffs;
    std::shared_ptr<pzdud_t> _engine;
};


Pothos::BufferManager::Sptr makeZynqDMABufferManager(std::shared_ptr<pzdud_t> engine, const pzdud_dir_t dir)
{
    if (dir == PZDUD_S2MM) return Pothos::BufferManager::Sptr(new ZynqDMABufferManager<PZDUD_S2MM>(engine));
    if (dir == PZDUD_MM2S) return Pothos::BufferManager::Sptr(new ZynqDMABufferManager<PZDUD_MM2S>(engine));
    return Pothos::BufferManager::Sptr();
}
