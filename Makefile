TARGET := vrr_core
obj-m := $(TARGET).o
KDIR := /lib/modules/$(shell uname -r)/build
INCLUDE := -isystem /lib/modules/`uname -r`/build/include
PWD := $(shell pwd)

default:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) modules
all:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) modules
clean:
	$(MAKE) -C $(OPT) $(KDIR) SUBDIRS=$(PWD) clean
