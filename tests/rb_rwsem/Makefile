# Makefile for rb_rwsem_test
obj-m += rb_rwsem_test.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# 使用内核编译后的rb_rwsem模块符号表
KBUILD_EXTRA_SYMBOLS := /lib/modules/$(shell uname -r)/kernel/drivers/PriChain_codes/rb_rwsem/Module.symvers

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 