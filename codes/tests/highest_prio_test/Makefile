obj-m += highest_prio_test.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

KBUILD_EXTRA_SYMBOLS := /lib/modules/$(shell uname -r)/kernel/drivers/codes/rb_mutex/Module.symvers

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean