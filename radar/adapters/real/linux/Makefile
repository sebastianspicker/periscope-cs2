# Kbuild Makefile for aclab_module.ko
# Build: make -C /lib/modules/$(uname -r)/build M=$(pwd) modules

obj-m += aclab_module.o

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

load:
	sudo insmod aclab_module.ko
	@echo "Module loaded. Device: /dev/aclab"
	@echo "ls -la /dev/aclab"
	@ls -la /dev/aclab

unload:
	sudo rmmod aclab_module.ko
	@echo "Module unloaded"

test: all load
	@echo "Testing aclab kernel module..."
	@echo "  - Checking /proc/modules:"
	@grep aclab /proc/modules || echo "  NOT FOUND"
	@echo "  - Checking /dev/aclab:"
	@ls -la /dev/aclab 2>/dev/null || echo "  NOT FOUND"
	@echo "  - dmesg tail:"
	@dmesg | tail -5
	$(MAKE) unload

.PHONY: all clean load unload test
