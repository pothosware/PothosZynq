// Copyright (c) 2013-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Framework.hpp>
#include "pothos_zynq_dma_driver.h"
#include <memory>

//! Factory for Zynq DMA buffer manager
Pothos::BufferManager::Sptr makeZynqDMABufferManager(std::shared_ptr<pzdud_t> engine, const pzdud_dir_t dir);
