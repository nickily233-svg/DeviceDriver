# module
obj-m += globalmem_concurrent_control.o
ccflags-y +=-I$(src)/..
# current module's dir
PWD := $(shell pwd)
# kernel build dir
KDIR ?= /lib/modules/$(shell uname -r)/build
# default target
all :
	$(MAKE) -C $(KDIR) M=$(PWD) modules
# clean
clean :
	$(MAKE) -C $(KDIR) M=$(PWD) clean