INSTALL_DIR ?= ../release
PWD ?= $(shell pwd)
GIT_VERSION = $(shell cd $(PWD) && git describe --always --dirty --long)

obj-m += i2c-bcm283x-rtdm.o 
i2c-bcm283x-rtdm-y := ../ksrc/bcm2835.o ../ksrc/i2c-bcm283x-rtdm.o
ccflags-y += -I$(KERNEL_DIR)/include/xenomai
ccflags-y += -DGIT_VERSION=\"$(GIT_VERSION)\"

.PHONY: all build clean install

all: build info install

build:
	make -C $(KERNEL_DIR) M=$(PWD) modules

info:
	modinfo i2c-bcm283x-rtdm.ko

install:
	@mkdir -p $(INSTALL_DIR)
	cp i2c-bcm283x-rtdm.ko $(INSTALL_DIR)/i2c-bcm283x-rtdm.ko

clean:
	@make -C $(KERNEL_DIR) M=$(PWD) clean
	@rm -f ../ksrc/*.o
	@rm -f ../ksrc/.*.o.*
