########################################################################
## Linux kernel module build for Pothos applications
########################################################################
ifndef
	KDIR := /lib/modules/$(shell uname -r)/build
endif

########################################################################
## sources for pothos_zynq_dma module
########################################################################
POTHOS_AXIS_DMA_SOURCES = \
	pothos_zynq_dma_irq.c \
	pothos_zynq_dma_fops.c \
	pothos_zynq_dma_alloc.c \
	pothos_zynq_dma_module.c

pothos_zynq_dma-objs = $(POTHOS_AXIS_DMA_SOURCES:.c=.o)

########################################################################
## list of modules built here
########################################################################
obj-m := pothos_zynq_dma.o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

########################################################################
## build and clean targets
########################################################################
all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
