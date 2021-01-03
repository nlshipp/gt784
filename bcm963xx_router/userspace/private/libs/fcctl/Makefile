default: dynamic


all:
	cp libfcctl.so_$(PROFILE)_save libfcctl.so

dynamic: all generic_private_lib_install

clean:
	rm -f libfcctl.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
LIB = libfcctl.so
