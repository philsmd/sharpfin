obj-m += reciva_ctx2050.o

reciva_ctx2050-objs := ctx2050.o ctx2050_spi.o ctx2050_bbm.o fci_tuner.o

all: modules

clean:
	rm -f *.mod.c *.mod *.o .*.cmd *.ko
	rm -rf $(PWD)/tmp

modules:
	$(MAKE) -C ../.. SUBDIRS=$(PWD) modules
