BUILD_DIR=../build/kernel
INCLUDE_DIR=include
USER_DIR=../user
USER_INIT_DIR=../user/init
KERNEL_DIR=.
USER_BUILD_DIR=../build/user

SRC := $(shell find $(KERNEL_DIR) -type f -name "*.c")
ASM_SRC := $(shell find $(KERNEL_DIR) -type f -name "*.S")

OBJS := $(patsubst $(KERNEL_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
OBJS += $(patsubst $(KERNEL_DIR)/%.S, $(BUILD_DIR)/%.o, $(ASM_SRC))
ENTRY_OBJ := $(BUILD_DIR)/asm/entry.o
OBJS := $(filter-out $(ENTRY_OBJ), $(OBJS))
OBJS := $(ENTRY_OBJ) $(OBJS) 

# CFLAGS += -D__DEBUG_SYS_EXEC
# CFLAGS += -D__DEBUG_NAMEX

# CFLAGS += -D__DEBUG_KILL
# CFLAGS += -D__DEBUG_PEXIT
# CFLAGS += -D__DEBUG_THREAD_EXIT
# CFLAGS += -D__DEBUG_SLEEPLOCK
# CFLAGS += -D__DEBUG_FORK
# CFLAGS += -D__DEBUG_ALLOCATE_PROC
# CFLAGS += -D__DEBUG_WAIT
# CFLAGS += -D__DEBUG_FILEWRITE
# CFLAGS += -D__DEBUG_FREE_THREAD
# CFLAGS += -D__DEBUG_TCSTATE
# CFLAGS += -D__DEBUG_UTRAP
# CFLAGS += -D__DEBUG_TRAP
# CFLAGS += -D__DEBUG_PUSH_OFF
# CFLAGS += -D__DEBUG_THREAD_SCHED
# CFLAGS += -D__DEBUG_PCSTATE
# CFLAGS += -D__DEBUG_SPINLOCK
# CFLAGS += -D__DEBUG_TSLEEP
# CFLAGS += -D__DEBUG_DO_MMAP
# CFLAGS += -D__DEBUG_MMAP_WRITEBACK
# CFLAGS += -D__DEBUG_SCHED
# CFLAGS += -D__DEBUG_EXEC
# CFLAGS += -D__DEBUG_FLOADSEG
# CFLAGS += -D__DEBUG_SYSCALL
# CFLAGS += -D__USE_XV6FS
# CFLAGS += -D__DEBUG_SYS_FSTAT
# CFLAGS += -D__DEBUG_EXT4_VFREAD
# CFLAGS += -D__DEBUG_EXT4_VFCLOSE
# CFLAGS += -D__DEBUG_CLOSE
# CFLAGS += -D__DEBUG_GOPEN
# CFLAGS += -D__DEBUG_SYS_OPENAT
# CFLAGS += -D__DEBUG_EXT4_VGENTDENTS
# CFLAGS += -D__DEBUG_DEV_BREAD
# CFLAGS += -D__DEBUG_BREAD
# CFLAGS += -D__DEBUG_FREEDESC
# CFLAGS += -D__DEBUG_VDISKRW
# CFLAGS += -D__DEBUG_DISK_INTR


.PHONY: all clean

LDFLAGS = -z max-page-size=4096
all: $(BUILD_DIR)/kernel

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/kernel: $(OBJS) $(KERNEL_DIR)/kernel.ld $(USER_BUILD_DIR)/initcode
	$(LD) $(LDFLAGS) -T $(KERNEL_DIR)/kernel.ld -o $(BUILD_DIR)/kernel $(OBJS)
	$(OBJDUMP) -S $(BUILD_DIR)/kernel > $(BUILD_DIR)/kernel.asm
	$(OBJDUMP) -t $(BUILD_DIR)/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BUILD_DIR)/kernel.sym

# 清理
clean:
	@rm -rf $(BUILD_DIR)
	@if [ -f $(INCLUDE_DIR)/sysnum.h ]; then rm $(INCLUDE_DIR)/sysnum.h; fi
	@if [ -f $(INCLUDE_DIR)/sysfunc.h ]; then rm $(INCLUDE_DIR)/sysfunc.h; fi
	@if [ -f $(INCLUDE_DIR)/sysdecl.h ]; then rm $(INCLUDE_DIR)/sysdecl.h; fi

