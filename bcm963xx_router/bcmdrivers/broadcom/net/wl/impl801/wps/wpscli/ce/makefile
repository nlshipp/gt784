#Linux Makefile
#Added by Suman
SRCBASE = ../../..

include $(SRCBASE)/GNUmakefile.inc

empty           :=
space           :=$(empty) $(empty)

TARGET_PROCS    := X86 ARM
TARGET_OSVERS   := 500 600
TARGETNAME = ce_wpscli
VALID_TARGETS   := $(foreach devos,$(TARGET_OSVERS),    \
                   $(foreach devproc,$(TARGET_PROCS),   \
                   ce$(devos)_$(devproc)))

TARGETS         ?=
TARGETS         += ce500_X86
TARGETS         += ce600_X86
TARGETS         += ce500_ARM
TARGETS         += ce600_ARM

ce_wpscli_lib: $(TARGETS)

$(TARGETS): DEVOS=$(subst ce,,$(word 1,$(subst _,$(space),$@)))
$(TARGETS): DEVPROC=$(word 2,$(subst _,$(space),$@))
$(TARGETS):
	$(MAKE) WINCEVER=$(DEVOS) PROCESSOR=$(DEVPROC) $(ACTION)

clean_wpscli_lib: ACTION=clean
clean_wpscli_lib: $(TARGETS)

.PHONY: ce_wpscli_lib clean_wpscli_lib
