obj-m += wm8721.o wm8721-i2c.o
obj-m += pt2314.o
obj-m += ak4370.o
obj-m += reciva_dummy_mixer.o
obj-m += reciva_dummy_mixer_2496.o
obj-m += wm8816.o

all: modules

clean:
	rm -f *.mod.c *.mod *.o .*.cmd *.ko
	rm -rf $(PWD)/tmp

modules:
	$(MAKE) -C ../.. SUBDIRS=$(PWD) modules
