# Makefile for rb_mutex_user: 测试rb_mutex外部模块死锁检测
obj-m := rb_mutex_user.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# 导入rb_mutex模块的符号表
 KBUILD_EXTRA_SYMBOLS := ../../../rb_mutex/Module.symvers

# EXTRA_CFLAGS += -I$(PWD)/../rb_mutex

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
