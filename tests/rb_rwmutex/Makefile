# Makefile for rb_rwmutex_test
obj-m += rb_rwmutex_test.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# 使用内核编译后的rb_rwmutex模块符号表
KBUILD_EXTRA_SYMBOLS := /lib/modules/$(shell uname -r)/kernel/drivers/codes/rb_rwmutex/Module.symvers

EXTRA_CFLAGS += -I$(PWD)/../../rb_mutex -I$(PWD)/../../rb_rwmutex

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean 