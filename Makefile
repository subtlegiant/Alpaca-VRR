obj-m := vrr.o
vrr-objs := vrr_mod.o vrr_core.o vrr_input.o af_vrr.o vrr_data.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) modules
all:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) clean
