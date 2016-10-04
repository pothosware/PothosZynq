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
## kernel module
########################################################################
MAKEFILE_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
SOURCE_DIR := $(realpath $(MAKEFILE_DIR)/..)
POTHOS_ZYNQ_KO := $(SOURCE_DIR)/kernel/pothos_zynq_dma.ko

$(POTHOS_ZYNQ_KO):
	make -C $(KDIR) M=$(PWD) modules

########################################################################
## deb target
########################################################################
POTHOS_ZYNQ_DEB_DIR := $(SOURCE_DIR)/pothos-zynq_0.1.0
POTHOS_ZYNQ_DEB := $(POTHOS_ZYNQ_DEB_DIR).deb
KERNELRELEASE := $(shell make -C $(KDIR) M=$(PWD) kernelrelease --no-print-directory)

$(POTHOS_ZYNQ_DEB): $(POTHOS_ZYNQ_KO)

	#setup the debian build directory
	@echo "KERNELRELEASE=$(KERNELRELEASE)"
	mkdir -p "$(POTHOS_ZYNQ_DEB_DIR)/DEBIAN"
	mkdir -p "$(POTHOS_ZYNQ_DEB_DIR)/usr/include"
	mkdir -p "$(POTHOS_ZYNQ_DEB_DIR)/lib/modules/$(KERNELRELEASE)/kernel/drivers/"
	mkdir -p "$(POTHOS_ZYNQ_DEB_DIR)/etc/modules-load.d"
	cp "$(SOURCE_DIR)/debian/"* "$(POTHOS_ZYNQ_DEB_DIR)/DEBIAN/"
	cp "$(SOURCE_DIR)/kernel/pothos_zynq_dma_common.h" "$(POTHOS_ZYNQ_DEB_DIR)/usr/include/"
	cp "$(SOURCE_DIR)/driver/pothos_zynq_dma_driver.h" "$(POTHOS_ZYNQ_DEB_DIR)/usr/include/"
	cp "$(POTHOS_ZYNQ_KO)" "$(POTHOS_ZYNQ_DEB_DIR)/lib/modules/$(KERNELRELEASE)/kernel/drivers/"
	echo "pothos_zynq_dma" > "$(POTHOS_ZYNQ_DEB_DIR)/etc/modules-load.d/pothos_zynq.conf"

	#build a deb
	cd "$(SOURCE_DIR)" && fakeroot dpkg-deb --build "$(POTHOS_ZYNQ_DEB_DIR)"
	rm -rf "$(POTHOS_ZYNQ_DEB_DIR)"

########################################################################
## build and clean targets
########################################################################
all: $(POTHOS_ZYNQ_KO)

deb: $(POTHOS_ZYNQ_DEB)

clean:
	make -C $(KDIR) M=$(PWD) clean
