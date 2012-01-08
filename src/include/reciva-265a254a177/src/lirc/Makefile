obj-m += lirc_dev.o
obj-m += lirc_barracuda.o

all: modules

clean:
	rm -f *.mod.c *.mod *.o .*.cmd *.ko
	rm -rf $(PWD)/tmp

modules:
	$(MAKE) -C ../.. SUBDIRS=$(PWD) modules
