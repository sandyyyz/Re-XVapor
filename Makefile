KERNEL_DIR=kernel
USER_DIR=user
MKFS_DIR=mkfs
BUILD_DIR=build
UPROGS_LIST = $(BUILD_DIR)/user/uprogs-list.mk

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2 -Wno-error=unused-but-set-variable
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -Iinclude 
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

QEMU = qemu-system-riscv64

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump




# include $(UPROGS_LIST)

export TOOLPREFIX
export CC
export AS
export LD
export OBJCOPY
export OBJDUMP
export CFLAGS




.PHONY: all user kernel qemu qemu-gdb clean

all: kernel user fs.img

kernel:	user
	if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR); fi
	$(MAKE) -C $(KERNEL_DIR)

user:
	if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR); fi
	$(MAKE) -C $(USER_DIR)

clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	$(MAKE) -C $(USER_DIR) clean
	@if [ -d $(BUILD_DIR) ]; then rm -r $(BUILD_DIR); fi
	@if [ -f mkfs/mkfs ]; then rm mkfs/mkfs; fi


mkfs/mkfs: mkfs/mkfs.c $(KERNEL_DIR)/include/fs.h $(KERNEL_DIR)/include/param.h 
	gcc $(XCFLAGS) -Werror -Wall -fno-freestanding -o mkfs/mkfs mkfs/mkfs.c




fs.img: mkfs/mkfs README  $(UEXTRA) $(UPROGS) $(UPROGS_TEST)
	$(MAKE) -C $(USER_DIR) all
	@mkdir -p build/fs
	mkfs/mkfs build/fs/fs.img README $(UEXTRA) $(UPROGS) $(UPROGS_TEST)

-include kernel/*.d user/*.d

-include $(BUILD_DIR)/user/uprogs-list.mk

# mk_uplist: $(UPROGS) $(UPROGS_TEST)
# 	$(MAKE) -C $(USER_DIR) uprogs-list.mk

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

ifndef CPUS
CPUS := 4
endif


QEMUOPTS = -machine virt -bios none -kernel $(BUILD_DIR)/kernel/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=$(BUILD_DIR)/fs/fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0


qemu: kernel fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(BUILD_DIR)/$(KERNEL_DIR)/kernel .gdbinit $(BUILD_DIR)/fs/fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)



