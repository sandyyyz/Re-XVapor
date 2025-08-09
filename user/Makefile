BUILD_DIR=../build/user
INITDIR=$(BUILD_DIR)/init
INCLUDE_DIR=include
KERNEL_DIR=../kernel
U=.
SUBDIRS=init shell src 
SUBDIR_PATHS := $(patsubst %, $(BUILD_DIR)/%, $(SUBDIRS))
UPROGS_BUILD_DIR=$(BUILD_DIR)/uprogs
ULIB_DIR=lib

SRC = $(wildcard *.c) $(foreach dir, $(SUBDIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC))
# UPROGS = $(patsubst $(BUILD_DIR)/%.o, $(BUILD_DIR)/%.prog,$(OBJ))
UPROGS = $(patsubst %.c, $(BUILD_DIR)/%.prog, $(SRC))
ULIBSRC = $(wildcard *.c) $(foreach dir, $(ULIB_DIR), $(wildcard $(dir)/*.c))
ULIB = $(patsubst %.c, $(BUILD_DIR)/%.o, $(ULIBSRC))
ULIB += $(BUILD_DIR)/$(ULIB_DIR)/usys.o
CFLAGS += -I$(KERNEL_DIR)/include 
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
# $(info src : $(SRC))
# $(info obj : $(OBJ))
# $(info uprogs : $(UPROGS))r
# $(info ulib : $(ULIB))default: all
.PHONY: clean all MKDIR

TESTING_PLATFORM = no
ifeq ($(TESTING_PLATFORM),yes)
USER_EXTRA_DEPS :=
else
USER_EXTRA_DEPS := $(KERNEL_DIR)/include/initcode.h
endif

ifeq ($(ARCH), riscv)
	USYSPL:=$U/usys.pl
	USERLD:=$U/user.ld
else
	USYSPL:=$U/loongarch_usys.pl
	USERLD:=$U/loongarch_user.ld
endif

all: user

uprogs-list.mk: $(UPROGS) $(UPROGS_TEST)
	@echo "UPROGS := $(UPROGS)" | sed 's|\.\./||g' > $(BUILD_DIR)/uprogs-list.mk
	@echo "UPROGS_TEST := $(UPROGS_TEST)" | sed 's|\.\./||g' >> $(BUILD_DIR)/uprogs-list.mk

MKDIR: $(SUBDIR_PATHS) 
	mkdir -p $(BUILD_DIR)/$(ULIB_DIR)


$(SUBDIR_PATHS):
	mkdir -p $@

user: MKDIR $(BUILD_DIR)/initcode $(OBJ) $(ULIB) $(UPROGS) uprogs-list.mk $(USER_EXTRA_DEPS)


$(BUILD_DIR)/%.prog: $(BUILD_DIR)/%.o $(ULIB) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T $(USERLD) -o $@ $<  $(ULIB)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# $(BUILD_DIR)/%: 
# 	@mkdir -p $(dir $@)

$(BUILD_DIR)/%.o: %.c 
	@mkdir -p $(dir $@)
	@echo "Building $@"
	$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c $< -o $@

$(BUILD_DIR)/initcode.o: $(U)/asm/initcode.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I./include -I../kernel/include -c $< -o $@

# $(BUILD_DIR)/initcode.out: $(INITDIR)/init.o $(ULIB)
# 	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $< $(ULIB)

# $(BUILD_DIR)/initcode.out: $(BUILD_DIR)/initcode.o
# 	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $@ $<

$(BUILD_DIR)/initcode: $(INITDIR)/init.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(BUILD_DIR)/initcode.out $^
	$(OBJCOPY) -S -O binary $(BUILD_DIR)/initcode.out $(BUILD_DIR)/initcode
	$(OBJDUMP) -S $(BUILD_DIR)/initcode.out > $(BUILD_DIR)/initcode.dump
# $(BUILD_DIR)/initcode: $(BUILD_DIR)/initcode.out
# 	$(OBJCOPY) -S -O binary $(BUILD_DIR)/initcode.out $(BUILD_DIR)/initcode

$(KERNEL_DIR)/include/initcode.h: $(BUILD_DIR)/initcode
	xxd -i $< | \
	sed 's/unsigned char .*\[\]/unsigned char initcode[]/' | \
	sed 's/unsigned int .*_len/unsigned int initcode_len/' > $@


$(BUILD_DIR)/$(ULIB_DIR)/usys.S : $(USYSPL)
	perl $< > $@

$(BUILD_DIR)/$(ULIB_DIR)/usys.o : $(BUILD_DIR)/$(ULIB_DIR)/usys.S
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.o
clean:
	@rm -rf $(BUILD_DIR)