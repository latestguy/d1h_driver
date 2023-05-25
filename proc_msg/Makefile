KERN_DIR = /home/public/work/riscv/nezha-stu/eLinuxCore_dongshannezhastu/linux

all: 
	make -C $(KERN_DIR) M=$(PWD) modules

clean:
	make -C $(KERN_DIR) M=$(PWD) modules clean
	rm -rf modules.order

obj-m += mymsg.o