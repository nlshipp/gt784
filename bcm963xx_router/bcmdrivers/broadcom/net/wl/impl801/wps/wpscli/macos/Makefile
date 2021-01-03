#
# Copyright (C) 2009 Broadcom Corporation
#
# Linux Makefile to build wpscli library
#
# $Id: Makefile,v 1.1 2010/08/05 21:59:03 ywu Exp $


SRCBASE    := ../../..
BLDTYPE   ?= debug
#BLDTYPE    ?= release
CC         ?= gcc
TARGETARCH ?= x86
OBJDIR     ?= obj/$(BLDTYPE)/$(TARGETARCH)

ifeq ($(BLDTYPE),debug)
  CFLAGS := -Wall -Wnested-externs -g -D_TUDEBUGTRACE -DDEBUG -D_REENTRANT
else
  CFLAGS := -Wall -Os -Wnested-externs -DWPS_WIRELESS_ENROLLEE
endif

WPSCLIBASEDIR:= $(SRCBASE)/wps/wpscli
WPSLIBDIR    := $(WPSCLIBASEDIR)/linux
WPSCOMDIR    := $(SRCBASE)/wps/common
CRYPTDIR     := $(SRCBASE)/bcmcrypto
WPSCLICOMMDIR:= ../common
WPSCOMMON    := wpscommon
WPSCLIOSL    := wpscliosl
WPSCLIAPI    := wpscliapi
BCMCRYPTO    := bcmcrypto
WPSTESTCMD   := wpstestcmd

SAMPLEAPPLIBS := libwpscli.a libbcmcrypto.a libwpscore.a libwpscliosl.a
#SAMPLEAPPLIBS = libwpscli.a
#SAMPLEAPPLIBS = libfoo.a
SAMPLEAPPLIBS := $(SAMPLEAPPLIBS:%=$(OBJDIR)/%)

INCLUDE :=  -I$(SRCBASE)/include \
		-I$(WPSCLIBASEDIR)/include \
		-I$(WPSCOMDIR)/include \
		-I../common/include \
		-I../include \
		-I$(SRCBASE)/include/proto \
		-I$(SRCBASE)/include/bcmcrypto

vpath %.c $(WPSCLICOMMDIR) \
	$(WPSLIBDIR) \
	$(CRYPTDIR) \
	$(WPSCOMDIR)/shared \
	$(WPSCOMDIR)/ap \
	$(WPSCOMDIR)/sta \
	$(WPSCOMDIR)/enrollee \
	$(WPSCOMDIR)/registrar \
	$(WPSTESTCMDSRCS)

# shared files
WPSCORESRCS  = buffobj.c reg_proto_msg.c reg_proto_utils.c slist.c
WPSCORESRCS += state_machine.c tlv.c tutrace.c
# ap files
WPSCORESRCS += ap_api.c ap_eap_sm.c ap_upnp_sm.c dev_config.c
# sta files
WPSCORESRCS += sta_eap_sm.c
# enrollee files
WPSCORESRCS +=enr_api.c enr_reg_sm.c
# registrar files
WPSCORESRCS += reg_sm.c

CRYPTSRCS    = aes.c rijndael-alg-fst.c dh.c bn.c sha256.c hmac_sha256.c
CRYPTSRCS   += random.c

OSLSRCS      = wpscli_osl_helper.c wpscli_osl_timer.c wpscli_pktdisp.c 
OSLSRCS     += wpscli_wl_handler.c wpscli_wlan.c

OSISRCS      = wpscli_api.c wpscli_softap.c wpscli_sta.c wpscli_wl.c
OSISRCS     += wpscli_wpscore_hooks.c

WPSCOREOBJS  = $(WPSCORESRCS:%.c=$(OBJDIR)/%.o)
CRYPTOBJS    = $(CRYPTSRCS:%.c=$(OBJDIR)/%.o)
OSLOBJS      = $(OSLSRCS:%.c=$(OBJDIR)/%.o)
OSIOBJS      = $(OSISRCS:%.c=$(OBJDIR)/%.o)

WPSTESTCMDSRCS = wpscli_test_cmd.c
WPSTESTCMDOBJS = $(WPSTESTCMDSRCS:%.c=$(OBJDIR)/%.o)

all: showinfo objdir 
all: $(OBJDIR)/libbcmcrypto.a $(OBJDIR)/libwpscore.a 
all: $(OBJDIR)/libwpscliosl.a $(OBJDIR)/libwpscli.a
all: $(OBJDIR)/wpscli_test_cmd
#all : objdir libfoo.a wpscli_test_cmd

showinfo:
	@echo "WPSCOREOBJS = $(WPSCOREOBJS)"
	@echo "CRYPTOBJS = $(CRYPTOBJS)"
	@echo "OSLOBJS = $(OSLOBJS)"
	@echo "OSIOBJS = $(OSIOBJS)"

# create folders to hold objects files of each lib
objdir :
	@[ -d "$(OBJDIR)" ] || mkdir -pv $(OBJDIR)

# WPS common files shared by both wpscli and AP
libwpscore.a $(OBJDIR)/libwpscore.a : $(WPSCOREOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) cr$(if $(V),v) $(OBJDIR)/$(@F) $^

# shared crypto library
libbcmcrypto.a $(OBJDIR)/libbcmcrypto.a : $(CRYPTOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) cr$(if $(V),v) $(OBJDIR)/$(@F) $^

# wpscli osl lib file
libwpscliosl.a $(OBJDIR)/libwpscliosl.a : $(OSLOBJS) $(CRYPTOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) cr$(if $(V),v) $(OBJDIR)/$(@F) $^

# final wpscli api lib file to be deliverred
libwpscli.a $(OBJDIR)/libwpscli.a : $(OSIOBJS) $(WPSCOREOBJS) $(OSLOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) cr$(if $(V),v) $(OBJDIR)/$(@F) $^

wpscli_test_cmd $(OBJDIR)/wpscli_test_cmd : $(WPSTESTCMDOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(CC) $(if $(V),-H) $(INCLUDE) $(WPSTESTCMDOBJS) $(SAMPLEAPPLIBS) \
		-lpthread -o $(OBJDIR)/$(@F)

$(WPSCOREOBJS) $(CRYPTOBJS) $(OSLOBJS) $(OSIOBJS) $(WPSTESTCMDOBJS): $(OBJDIR)/%.o: %.c
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(CC) -c $(CFLAGS) $(if $(V),-H) $(INCLUDE) $< -o $@

# cleanup
clean: cleanfiles cleanfolders

cleanfiles:
	find $(OBJDIR) -name "*.o" | xargs rm -f
	find $(OBJDIR) -name "*.a" | xargs rm -f
	find $(OBJDIR) -name "wpscli_test_cmd"  | xargs rm -f

cleanfolders:
	@rm -rfv $(OBJDIR)

libfoo.a : $(OBJDIR)/foo.o
	$(AR) cr $@ $^
