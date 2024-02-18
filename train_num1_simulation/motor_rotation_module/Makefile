obj-m += motor_rotation_module.o
KDIR = /lib/modules/$(shell uname -r)/build
all:
	make -C $(KDIR) M=$(shell pwd) modules M=/home/pi/Desktop/motor_rotation_module motor_rotation_module
clean:
	make -C $(KDIR) M=$(shell pwd) clean