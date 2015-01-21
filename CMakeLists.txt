########################################################################
## Project setup
########################################################################
cmake_minimum_required(VERSION 2.8)
project(PothosZynq)
enable_language(CXX C)
enable_testing()

find_package(Pothos CONFIG REQUIRED)

########################################################################
## Build module for Zynq DMA blocks
########################################################################
include_directories(${PROJECT_SOURCE_DIR}/kernel)
include_directories(${PROJECT_SOURCE_DIR}/driver)
include_directories(${PROJECT_SOURCE_DIR}/blocks)

set(SOURCES
    blocks/ZynqDMASource.cpp
    blocks/ZynqDMASink.cpp
    blocks/ZynqBufferManager.cpp
    blocks/TestZynqDMALoopback.cpp
)

POTHOS_MODULE_UTIL(
    TARGET ZynqSupport
    SOURCES ${SOURCES}
    DESTINATION blocks/zynq
    ENABLE_DOCS
)

########################################################################
## Install driver header files
########################################################################
install(FILES
    ${PROJECT_SOURCE_DIR}/driver/pothos_zynq_dma_driver.h
    ${PROJECT_SOURCE_DIR}/kernel/pothos_zynq_dma_common.h
    DESTINATION include
)
