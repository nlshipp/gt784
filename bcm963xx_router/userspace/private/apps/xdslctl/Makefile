dynamic: all generic_exe_install

clean:
	rm -f xdslctl


all:
	cp xdslctl_$(PROFILE)_save xdslctl

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
EXE = xdslctl
