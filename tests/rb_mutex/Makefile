# Makefile for rb_mutex_test
obj-m += rb_mutex_test.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# 使用内核编译后的rb_mutex模块符号表
KBUILD_EXTRA_SYMBOLS := /lib/modules/$(shell uname -r)/kernel/drivers/codes/rb_mutex/Module.symvers

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# 安装测试模块
install:
	@echo "Installing rb_mutex_test.ko"
	@sudo insmod rb_mutex_test.ko

# 卸载测试模块
uninstall:
	@echo "Uninstalling rb_mutex_test.ko"
	@sudo rmmod rb_mutex_test 2>/dev/null || true