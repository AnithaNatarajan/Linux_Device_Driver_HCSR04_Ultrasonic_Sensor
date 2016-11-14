
IOT_HOME =/opt/iot-devkit/1.7.3/sysroots/
KDIR:=$(IOT_HOME)/i586-poky-linux/usr/src/kernel  
PATH := $(PATH):$(IOT_HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux
CROSS_COMPILE =i586-poky-linux-
SROOT=$(IOT_HOME)/i586-poky-linux/
CC =i586-poky-linux-gcc

ARCH = x86

APP = test1

obj-m := hcsr04_platform_device.o
obj-m += hcsr_platform_driver.o


all:
	make ARCH=$(ARCH) CROSS_COMPILE=i586-poky-linux- -C $(KDIR) M=$(PWD) modules
	i586-poky-linux-gcc -o $(APP) test1.c --sysroot=$(SROOT)

clean:
	rm -f *.ko
	rm -f *.o
	rm -f Module.symvers
	rm -f modules.order
	rm -f *.mod.c
	rm -rf .tmp_versions
	rm -f *.mod.c
	rm -f *.mod.o
	rm -f \.*.cmd
	rm -f Module.markers
	rm -f $(APP) 


