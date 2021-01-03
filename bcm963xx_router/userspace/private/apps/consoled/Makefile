dynamic: all generic_exe_install

clean:
	rm -f consoled


all:
	cp consoled_$(PROFILE)_save consoled

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
EXE = consoled
