BUILD_DIR=../build/user
INCLUDE_DIR=include
KERNEL_DIR=../kernel
U=.
SUBDIRS=init shell src 
SUBDIR_PATHS := $(patsubst %, $(BUILD_DIR)/%, $(SUBDIRS))

ULIB_DIR=lib

SRC = $(wildcard *.c) $(foreach dir, $(SUBDIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC))
UPROGS = $(patsubst $(BUILD_DIR)/%.o, $(BUILD_DIR)/%.prog,$(OBJ))
ULIBSRC = $(wildcard *.c) $(foreach dir, $(ULIB_DIR), $(wildcard $(dir)/*.c))
ULIB = $(patsubst %.c, $(BUILD_DIR)/%.o, $(ULIBSRC))
CFLAGS += -I$(KERNEL_DIR)/include 
default: all
.PHONY: clean all

all: MKDIR $(BUILD_DIR)/initcode $(OBJ) uprogs-list.mk

uprogs-list.mk: $(UPROGS) $(UPROGS_TEST)
	@echo "UPROGS := $(UPROGS)" > $(BUILD_DIR)/uprogs-list.mk
	@echo "UPROGS_TEST := $(UPROGS_TEST)" >> $(BUILD_DIR)/uprogs-list.mk

MKDIR: $(SUBDIR_PATHS)
	
$(SUBDIR_PATHS):
	mkdir -p $@

$(BUILD_DIR)/%.prog: %.o $(ULIB)
	$(LD) $(LDFLAGS) -T $U/user.ld -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%: 
	@mkdir -p $(dir $@)

%.o: $(U)%.c | $(BUILD_DIR) $(ULIB) MKDIR
	$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c $< -o $@

$(BUILD_DIR)/initcode.o: $(U)/asm/initcode.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I./include -I../kernel/include -c $< -o $@

$(BUILD_DIR)/initcode.out: $(BUILD_DIR)/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $@ $<

$(BUILD_DIR)/initcode: $(BUILD_DIR)/initcode.out
	@cp $< $@



clean:
	@rm -rf $(BUILD_DIR)