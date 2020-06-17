#内核源码根目录
KERN_DIR = 

all:
	make -C $(KERN_DIR) M=`pwd` modules 
	$(CROSS_COMPILE)gcc -o stepMotor_test stepMotor_test.c 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order
	rm -f stepMotor_test

obj-m	+= stepMotor_drv.o
