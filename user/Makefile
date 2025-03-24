BUILD_DIR=../build/user
INCLUDE_DIR=include
KERNEL_DIR=../kernel
U=.
SUBDIRS=init shell src 
SUBDIR_PATHS := $(patsubst %, $(BUILD_DIR)/%, $(SUBDIRS))
UPROGS_BUILD_DIR=$(BUILD_DIR)/uprogs
ULIB_DIR=lib
UTEST_DIR = user/test
SRC = $(wildcard *.c) $(foreach dir, $(SUBDIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRC))
UPROGS = $(patsubst $(BUILD_DIR)/%.o, $(BUILD_DIR)/%.prog,$(OBJ))
ULIBSRC = $(wildcard *.c) $(foreach dir, $(ULIB_DIR), $(wildcard $(dir)/*.c))
ULIB = $(patsubst %.c, $(BUILD_DIR)/%.o, $(ULIBSRC))
CFLAGS += -I$(KERNEL_DIR)/include 
ULIB += $(BUILD_DIR)/$(ULIB_DIR)/usys.o
# $(info src : $(SRC))
# $(info obj : $(OBJ))
# $(info uprogs : $(UPROGS))r
# $(info ulib : $(ULIB))

UPROGS_TEST = \
  $(UTEST_DIR)/clone \
  $(UTEST_DIR)/wait \
  $(UTEST_DIR)/test_echo \
  $(UTEST_DIR)/execve \
  $(UTEST_DIR)/brk \
  $(UTEST_DIR)/mmap
default: all
.PHONY: clean all


all: user

uprogs-list.mk: 
	@echo "UPROGS := $(UPROGS)" | sed 's|\.\./||g' > $(BUILD_DIR)/uprogs-list.mk
	@echo "UPROGS_TEST := $(UPROGS_TEST)" | sed 's|\.\./||g' >> $(BUILD_DIR)/uprogs-list.mk

MKDIR: $(SUBDIR_PATHS) 
	mkdir -p $(BUILD_DIR)/$(ULIB_DIR)


$(SUBDIR_PATHS):
	mkdir -p $@

user: MKDIR $(BUILD_DIR)/initcode $(OBJ) $(ULIB) $(UPROGS) uprogs-list.mk

$(BUILD_DIR)/%.prog: %.o $(ULIB) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T $U/user.ld -o $@ $^

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# $(BUILD_DIR)/%: 
# 	@mkdir -p $(dir $@)

$(BUILD_DIR)%.o: $(U)%.c 
	@echo "Building $@"
	$(CC) $(CFLAGS) -I$(KERNEL_DIR)/include -c $< -o $@

$(BUILD_DIR)/initcode.o: $(U)/asm/initcode.S | $(BUILD_DIR)
	@echo "Building $@"
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I./include -I../kernel/include -c $< -o $@

$(BUILD_DIR)/initcode.out: $(BUILD_DIR)/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $@ $<

$(BUILD_DIR)/initcode: $(BUILD_DIR)/initcode.out
	@cp $< $@

$(BUILD_DIR)/$(ULIB_DIR)/usys.S : $U/usys.pl
	perl $< > $@

$(BUILD_DIR)/$(ULIB_DIR)/usys.o : $(BUILD_DIR)/$(ULIB_DIR)/usys.S
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.o
clean:
	@rm -rf $(BUILD_DIR)