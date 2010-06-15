
obj-m := vrr_core.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) modules
