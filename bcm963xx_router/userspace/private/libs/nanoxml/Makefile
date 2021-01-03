default: dynamic


all:
	cp libnanoxml.so_$(PROFILE)_save libnanoxml.so

dynamic: all generic_private_lib_install

clean:
	rm -f libnanoxml.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
LIB = libnanoxml.so
