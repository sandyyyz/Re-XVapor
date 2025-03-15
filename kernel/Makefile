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
