obj-m += rtmutex_test.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *.ko *.o *.mod.c modules.order Module.symvers
	rm -rf .tmp_versions 