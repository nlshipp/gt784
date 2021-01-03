default: dynamic


all:
	cp libmcasthal.so_$(PROFILE)_save libmcasthal.so

dynamic: all generic_private_lib_install

clean:
	rm -f libmcasthal.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
LIB = libmcasthal.so
