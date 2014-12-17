obj-m += procinfo.o

all:
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	make -C /usr/src/kernels/2.6.15-1.2054_FC5-i686 M=$(PWD) modules

clean:
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	make -C /usr/src/kernels/2.6.15-1.2054_FC5-i686 M=$(PWD) clean
