KERNEL_DIR=kernel
USER_DIR := user
MKFS_DIR=mkfs
BUILD_DIR=build
UPROGS_LIST = $(BUILD_DIR)/user/uprogs-list.mk
UTEST_DIR = user/test
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2 -Wno-error=unused-but-set-variable -Wno-error=format
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -Iinclude 
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
QEMU = qemu-system-riscv64

# FS_XV6FS = 1
ifdef FS_XV6FS
FSIMG := $(BUILD_DIR)/fs/fs.img
else
FSIMG := sdcard-rv.img
endif
UPROGS =

test: $(UPROGS_TEST)
	@echo "UPROGS_TEST: $(UPROGS_TEST)"
	@echo "UPROGS: $(UPROGS)"
	@echo "BUILD_DIR: $(BUILD_DIR)"
	@echo "USER_DIR: $(USER_DIR)"
	@echo "KERNEL_DIR: $(KERNEL_DIR)"
	@echo "UEXTRA: $(UEXTRA)"
	@echo "UTEST_DIR: $(UTEST_DIR)"
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
export UPROGS

.PHONY: all user kernel qemu qemu-gdb clean
.default: kernel

all: kernel user fs.img syscall_gen

SYSTBL=scripts/syscall.tbl
SYSDECL=kernel/include/sysdecl.h
SYSNUM=kernel/include/sysnum.h
SYSFUNC=kernel/include/sysfunc.h
SYSNAME=kernel/include/sysname.h

USYSPL=user/usys.pl
syscall_gen:
	@echo "Generating syscall files..."
	./scripts/sysgen.sh $(SYSTBL) $(SYSNUM) $(SYSFUNC) $(SYSDECL) $(USYSPL) $(SYSNAME)
	@echo "Generating syscall files done."
kernel:	user syscall_gen
	if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR); fi
	$(MAKE) -C $(KERNEL_DIR)

user: syscall_gen
	if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR); fi
	$(MAKE) -C $(USER_DIR)

clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	$(MAKE) -C $(USER_DIR) clean
	@if [ -d $(BUILD_DIR) ]; then rm -r $(BUILD_DIR); fi
	@if [ -f mkfs/mkfs ]; then rm mkfs/mkfs; fi

mkfs/mkfs: mkfs/mkfs.c $(KERNEL_DIR)/include/xv6fs.h $(KERNEL_DIR)/include/param.h 
	gcc -Werror -Wall -fno-freestanding -o mkfs/mkfs mkfs/mkfs.c

# $(USER_BUILD_DIR)/uprogs-list.mk:
# 	$(MAKE) -C $(USER_DIR) uprogs-list.mk

fs.img: $(UPROGS_TEST) $(UEXTRA) $(UPROGS) mkfs/mkfs README  
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


QEMUOPTS = -machine virt -bios default -kernel $(BUILD_DIR)/kernel/kernel -m 1G -smp $(CPUS) -nographic
# QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=$(FSIMG),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMUOPTS += -device virtio-net-device,netdev=net -netdev user,id=net


qemu: user kernel 
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: kernel .gdbinit $(FSIMG)
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)


