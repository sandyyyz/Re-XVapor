KERNEL_DIR=kernel
USER_DIR := user
MKFS_DIR=mkfs
BUILD_DIR=build
UPROGS_LIST = $(BUILD_DIR)/user/uprogs-list.mk
UTEST_DIR = user/test

ARCHS:= riscv loongarch

ARCH ?= riscv
export ARCH

ifeq ($(ARCH), riscv)
	CFLAGS = -Wall -Werror -O0 -fno-omit-frame-pointer -ggdb -gdwarf-2 -Wno-error=unused-but-set-variable -Wno-error=format
	CFLAGS += -MD
	CFLAGS += -mcmodel=medany
	CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
	CFLAGS += -Iinclude 
	CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
    CFLAGS += -D__ARCH_RISCV
	CFLAGS += -D__VIRTIO
else ifeq ($(ARCH), loongarch)
	CFLAGS = -Wall -O0 -fno-omit-frame-pointer -ggdb
	# CFLAGS += -Werror
	CFLAGS += -MD
	CFLAGS += -march=loongarch64 -mabi=lp64f
	CFLAGS += -ffreestanding -fno-common -nostdlib
	CFLAGS += -I. -fno-stack-protector
	CFLAGS += -Iinclude/
	CFLAGS += -fno-pie -no-pie
    CFLAGS += -D__ARCH_LOONGARCH
	CFLAGS += -D__CONFIG_2K1000LA
	CFLAGS += -D__AHCI
else
    $(error Unsupported ARCH: $(ARCH))
endif

FSIMG := sdcard-rv.img
FSIMG-LA := sdcard-la.img
UPROGS =

# Try to infer the correct TOOLPREFIX if not set
ifeq ($(ARCH), riscv)
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
else
# TOOLPREFIX := loongarch64-unknown-linux-gnu-
TOOLPREFIX := loongarch64-linux-gnu-
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

.PHONY: all user kernel qemu-rv qemu-la qemu-gdb qemu-gdb-la clean riscv loongarch
.default: all

all: $(ARCHS:%=build-%)

riscv: build-riscv
loongarch: build-loongarch

build-%: 
	$(MAKE) ARCH=$* user kernel syscall_gen

SYSTBL=scripts/syscall.tbl
SYSDECL=kernel/include/sysdecl.h
SYSNUM=kernel/include/sysnum.h
SYSFUNC=kernel/include/sysfunc.h
SYSNAME=kernel/include/sysname.h

USYSPL=user/usys.pl
USYSPL_LOONGARCH=user/loongarch_usys.pl

syscall_gen:
	@echo "Generating syscall files..."
	./scripts/sysgen.sh $(SYSTBL) $(SYSNUM) $(SYSFUNC) $(SYSDECL) $(USYSPL) $(SYSNAME) $(USYSPL_LOONGARCH)
	@echo "Generating syscall files done."	
kernel:	user syscall_gen
	if [ ! -d $(BUILD_DIR) ]; then mkdir $(BUILD_DIR); fi
	$(MAKE) -C $(KERNEL_DIR)
	$(MAKE) clean

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

fs.img: $(UPROGS_TEST) $(UEXTRA) $(UPROGS) README  
	$(MAKE) -C $(USER_DIR) all
	@mkdir -p build/fs

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
CPUS := 1
endif

QEMU = qemu-system-riscv64
QEMUOPTS = -machine virt -bios default -kernel kernel-rv -m 1G -smp $(CPUS) -nographic
# QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=$(FSIMG),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -no-reboot 
# QEMUOPTS += -device virtio-net-device,netdev=net -netdev user,id=net -rtc base=utc

# loongarch qemu command line

# qemu-system-loongarch64 -kernel {os_file} -m {mem} -nographic -smp {smp} -drive file={fs},if=none,format=raw,id=x0  \
#                         -device virtio-blk-pci,drive=x0,bus=virtio-mmio-bus.0 -no-reboot  -device virtio-net-pci,netdev=net0 \
#                         -netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=udp::5555-:5555  \
#                         -rtc base=utc \
#                         -drive file=disk-la.img,if=none,format=raw,id=x1 -device virtio-blk-pci,drive=x1,bus=virtio-mmio-bus.1

# QEMU-LA = qemu-system-loongarch64
# QEMU-LA = ../qemu-la-20240526/bin/qemu-system-loongarch64
QEMU-LA = ../qemu-la-20240401/bin/qemu-system-loongarch64
QEMUOPTS-LA = -kernel kernel-la -m 1G -nographic -smp $(CPUS) -drive file=$(FSIMG-LA),if=none,format=raw,id=x0
QEMUOPTS-LA += -device virtio-blk-pci,drive=x0 -no-reboot
QEMUOPTS-LA += -M ls2k
# QEMUOPTS-LA += -machine dumpdtb=loongarch64.dtb
# QEMUOPTS-LA += -monitor stdio
# QEMUOPTS-LA += -device virtio-blk-pci,drive=x0,bus=virtio-mmio-bus.0 -no-reboot
# QEMUOPTS-LA += -device virtio-net-pci,netdev=net0
# QEMUOPTS-LA += -netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=udp::5555-:5555 -rtc base=utc
# QEMUOPTS-LA += -drive file=disk-la.img,if=none,format=raw,id=x1 -device virtio-blk-pci,drive=x1,bus=virtio-mmio-bus.1

qemu-rv: .gdbinit
	$(QEMU) $(QEMUOPTS)

qemu-la: .gdbinit-la
	$(QEMU-LA) $(QEMUOPTS-LA)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

.gdbinit-la: .gdbinit.tmpl-loongarch
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: .gdbinit $(FSIMG)
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

qemu-gdb-la: .gdbinit-la $(FSIMG-LA)
	@echo "*** Now run 'loongarch64-unknown-linux-gnu-gdb' in another window." 1>&2
	$(QEMU-LA) $(QEMUOPTS-LA) -S -gdb tcp::$(GDBPORT)


