TARGET := vrr
obj-m := $(TARGET).o
vrr-objs := vrr_core.o vrr_input.o af_vrr.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) modules
all:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) clean
