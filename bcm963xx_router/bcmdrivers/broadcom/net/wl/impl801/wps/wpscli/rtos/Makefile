#
# GNUmakefile
#
# Copyright (c) 2009, Broadcom Corp.
# $Id: Makefile,v 1.1 2010/08/05 21:59:03 ywu Exp $


ifeq ($(SRCBASE),)
   SRCBASE := ../../..
endif

SUPPORTED_TARGETENVS := nucleusarm

# Error check that target environment is valid.
ifeq ($(filter $(TARGETENV),$(SUPPORTED_TARGETENVS)),)
$(warning Unsupported value for TARGETENV: '$(TARGETENV)')
$(warning TARGETENV must be one of the following:)
$(warning $(space))
$(foreach target,$(SUPPORTED_TARGETENVS),$(warning $(space)  $(target)))
$(error $(space))
endif

# Set output directory.
OBJDIR := $(TARGETENV)/$(TARGETCPU)/

# Include generic environment makefile.
include $(SRCBASE)/Makerules

WPSCLIBASEDIR:= $(SRCBASE)/wps/wpscli
WPSLIBDIR    := $(WPSCLIBASEDIR)/rtos
WPSCOMDIR    := $(SRCBASE)/wps/common
WPSRTOSDIR    := $(SRCBASE)/wps/rtos
CRYPTDIR     := $(SRCBASE)/bcmcrypto
WPSCLICOMMDIR:= ../common

vpath %.c $(WPSCLICOMMDIR) \
	$(WPSLIBDIR) \
	$(CRYPTDIR) \
	$(WPSCOMDIR)/shared \
	$(WPSCOMDIR)/ap \
	$(WPSCOMDIR)/sta \
	$(WPSCOMDIR)/enrollee \
	$(WPSCOMDIR)/registrar \
	$(WPSRTOSDIR)/enr

CFLAGS += -D_REENTRANT -DWPS_WIRELESS_ENROLLEE

# debug
CFLAGS += -g -D_TUDEBUGTRACE -DDEBUG

# include path
CFLAGS += -I$(SRCBASE)/include \
		-I$(WPSCLIBASEDIR)/include \
		-I$(WPSCOMDIR)/include \
		-I../common/include \
		-I../include \
		-I$(SRCBASE)/include/proto \
		-I$(SRCBASE)/include/bcmcrypto

# shared
CFILES := buffobj.c \
		reg_proto_msg.c \
		reg_proto_utils.c \
		slist.c \
		state_machine.c \
		tlv.c tutrace.c

# ap
CFILES += ap_api.c \
		ap_eap_sm.c \
		ap_upnp_sm.c \
		dev_config.c

# sta
CFILES += sta_eap_sm.c

# enrollee
CFILES +=enr_api.c \
		enr_reg_sm.c

# registrar
CFILES += reg_sm.c

# crypto
CFILES += aes.c \
		rijndael-alg-fst.c \
		dh.c bn.c \
		sha256.c \
		hmac_sha256.c \
		random.c

# osl
CFILES += wpscli_osl_helper.c \
		wpscli_osl_timer.c \
		wpscli_pktdisp.c \
		wpscli_wl_handler.c \
		wpscli_wlan.c \
		wps_rtos_hooks.c

# osi
CFILES += wpscli_api.c \
		wpscli_softap.c \
		wpscli_sta.c \
		wpscli_wl.c

DEP_FILES := $(wildcard $(OBJDIR)/*.d)
OBJ_FILES := $(addprefix $(OBJDIR),$(CFILES:.c=.o))
   
TARGETLIB = $(OBJDIR)/libwpscli.a
   
# Create output directory.
$(shell mkdir -p $(OBJDIR))
   
.PHONY: all
all: libs

.PHONY: libs
libs: libwpscli

.PHONY: libwpscli
libwpscli: $(TARGETLIB)
$(TARGETLIB): $(OBJ_FILES) 
	$(AR) $(TARGETLIB) $^

.PHONY: clean
clean:
	rm -Rf $(OBJDIR)

# Include dependencies.
ifeq ($(strip $(filter clean% clobber%, $(MAKECMDGOALS))),)
   ifneq ($(DEP_FILES),)
      include $(DEP_FILES)
   endif
endif
