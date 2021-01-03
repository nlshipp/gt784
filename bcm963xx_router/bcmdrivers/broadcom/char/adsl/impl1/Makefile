# File: bcmdrivers/char/bcm963xx/adsl/Makefile
#
# Makefile for the BCM63xx ADSL connection driver.
#

obj-$(CONFIG_BCM_ADSL) += adsldd.o

hmi-objs := BcmXdslHmi.o BcmHmiMsgReply.o bcm_hmiMsgHdr.o bcm_hmiCoreMsg.o bcm_bigEndianByteSwap.o bcm_layout.o bcm_hmiLineMsg.o

softdsl-objs += softdsl/SoftDslFrame.o softdsl/HdlcFramer.o softdsl/HdlcByte.o softdsl/G997.o 
softdsl-objs += softdsl/Flatten.o softdsl/DslFramer.o softdsl/AdslMibObj.o softdsl/AdslMib.o softdsl/G992p3OvhMsg.o
softdsl-objs += softdsl/BlockShortMove.o softdsl/BlockLongMove.o softdsl/BlockByteMove.o softdsl/BlockByteFill.o softdsl/ASSERT_SIZE.o

adsldd-objs += adsldrv.o adsl.o AdslCore.o AdslCoreFrame.o BcmAdslCore.o BcmAdslDiagLinux.o BcmOs.o AdslFile.o BcmAdslGdbStub.o
adsldd-objs += $(softdsl-objs)

#EXTRA_CFLAGS += -Wframe-larger-than=2048

ifeq "$(BRCM_PHY_BONDING)" "y"
EXTRA_CFLAGS += -DSUPPORT_DSL_BONDING
endif

ifeq "$(HMI_SUPPORT)" "y"
EXTRA_CFLAGS += -DHMI_QA_SUPPORT
adsldd-objs += $(hmi-objs)
endif

ifeq "$(BUILD_VECTORINGD)" "y"
EXTRA_CFLAGS += -DSUPPORT_VECTORINGD
endif

ifeq "$(ADSL_PHY_MODE)" "file"
EXTRA_CFLAGS += -DADSL_PHY_FILE
endif

ifeq "$(BRCM_CHIP)" "6338"
BRCM_CHIP=6348
endif
ifeq "$(BRCM_CHIP)" "6358"
BRCM_CHIP=6348
endif

ifeq "$(BRCM_CHIP)" "6328"
BRCM_CHIP=6362
endif

ifeq ($(strip $(BRCM_NTR_SUPPORT)),1)
EXTRA_CFLAGS += -DNTR_SUPPORT
endif

ifeq "$(ADSL)" "ANNEX_C"
phy_annex = C
EXTRA_CFLAGS += -DADSL_ANNEXC
HOST_EXTRACFLAGS += -DADSL_ANNEXC
endif
ifeq "$(ADSL)" "ANNEX_B"
phy_annex = B
EXTRA_CFLAGS += -DADSL_ANNEXB
HOST_EXTRACFLAGS += -DADSL_ANNEXB
endif
ifeq "$(ADSL)" "SADSL"
phy_annex = SA
EXTRA_CFLAGS += -DADSL_SADSL
HOST_EXTRACFLAGS += -DADSL_SADSL
endif
ifeq "$(ADSL)" "ANNEX_A"
phy_annex =
endif

ifeq "$(BRCM_CHIP)" "6362"
EXTRA_CFLAGS += -DSUPPORT_STATUS_BACKUP
endif

ifeq "$(BRCM_CHIP)" "6368"
EXTRA_CFLAGS += -DSUPPORT_STATUS_BACKUP -DCONFIG_VDSL_SUPPORTED
ifeq "$(BUILD_PHY_ADSL)" "y"
phy_mode = ao
endif
ifeq "$(BRCM_PHY_BONDING)" "y"
phy_bnd = bnd
endif
endif

ifeq "$(BRCM_CHIP)" "63268"
EXTRA_CFLAGS += -DSUPPORT_STATUS_BACKUP -DLMEM_ACCESS_WORKAROUND -DCONFIG_VDSL_SUPPORTED
ifeq "$(BUILD_PHY_ADSL)" "y"
phy_mode = ao
endif
ifeq "$(BRCM_PHY_BONDING)" "y"
phy_bnd = bnd
endif
endif

adslphy := adslcore$(BRCM_CHIP)$(phy_annex)$(phy_mode)$(phy_bnd)

ifneq ($(strip $(AEI_VDSL_CUSTOMER_NCS)),)
ifeq "$(BRCM_PHY_BONDING)" "y"
adslphy2 := adslcore$(BRCM_CHIP)ao$(phy_bnd)
adslphy3 := adslcore$(BRCM_CHIP)$(phy_annex)$(phy_mode)
endif
endif

ifeq "$(ADSL_SELF_TEST)" "y"
adslphy := adslcore$(BRCM_CHIP)Test
EXTRA_CFLAGS += -DADSL_SELF_TEST
endif

ifneq ($(strip $(BUILD_TR69_XBRCM)),)
EXTRA_CFLAGS += -DADSL_MIBOBJ_PLN
endif

#EXTRA_CFLAGS += -DUSE_LOCAL_DS_POWER
#EXTRA_CFLAGS += -DUSE_CXSY_OVH_MSG_WORKAROUND
EXTRA_CFLAGS += -DUSE_CXSY_OVH_MSG_COUNTER_WORKAROUND
EXTRA_CFLAGS += -DUSE_CXSY_OVH_MSG_DISABLE_POLLING_CMD

ifneq ($(strip $(BUILD_SNMP_EOC)),)
EXTRA_CFLAGS += -DBUILD_SNMP_EOC
endif
ifneq ($(strip $(BUILD_SNMP_AUTO)),)
EXTRA_CFLAGS += -DBUILD_SNMP_AUTO
endif
ifeq "$(BUILD_SNMP_TRANSPORT_DEBUG)" "y"
EXTRA_CFLAGS += -DBUILD_SNMP_TRANSPORT_DEBUG
endif

EXTRA_CFLAGS += -DLINUX_FW_VERSION=$(BRCM_VERSION)$(BRCM_RELEASE)

ifeq "$(BRCM_CHIP)" "6368"
EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)
else
EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)
endif

EXTRA_CFLAGS +=-fms-extensions
#EXTRA_CFLAGS +=-DPHY_LOOPBACK

EXTRA_CFLAGS += ${BRCM_DRIVER_ATM_BONDING_DEFINES}

ifeq "$(ADSL_PHY_MODE)" "file"

HOST_EXTRACFLAGS += -m32 -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD)

ifeq "$(ADSL_SELF_TEST)" "y"
PHYBLD_EXE := $(obj)/$(adslphy)/AdslTestPhyBld
PHYBLD_C := $(obj)/AdslTestPhyBld.c
PHY_BIN  := $(obj)/adsl_test_phy.bin

ifneq ($(strip $(AEI_VDSL_CUSTOMER_NCS)),)
PHYBLD_EXE2 := $(obj)/$(adslphy2)/AdslTestPhyBld
PHYBLD_C2 := $(obj)/AdslPhyBld.c
PHY_BIN2  := $(obj)/adsl_phy2.bin

PHYBLD_EXE3 := $(obj)/$(adslphy3)/AdslTestPhyBld
PHYBLD_C3 := $(obj)/AdslPhyBld.c
PHY_BIN3  := $(obj)/adsl_phy3.bin
endif
else
PHYBLD_EXE := $(obj)/$(adslphy)/AdslPhyBld
PHYBLD_C := $(obj)/AdslPhyBld.c
PHY_BIN  := $(obj)/adsl_phy.bin

ifneq ($(strip $(AEI_VDSL_CUSTOMER_NCS)),)
ifeq "$(BRCM_PHY_BONDING)" "y"
PHYBLD_EXE2 := $(obj)/$(adslphy2)/AdslPhyBld
PHYBLD_C2 := $(obj)/AdslPhyBld.c
PHY_BIN2  := $(obj)/adsl_phy2.bin

PHYBLD_EXE3 := $(obj)/$(adslphy3)/AdslPhyBld
PHYBLD_C3 := $(obj)/AdslPhyBld.c
PHY_BIN3  := $(obj)/adsl_phy3.bin
endif
endif
endif

clean-files := adsl_phy.bin adsl_test_phy.bin
clean-files += $(PHYBLD_EXE)

$(PHY_BIN): $(PHYBLD_EXE) 
	$(PHYBLD_EXE) $(PHY_BIN)
	if test ! -f $(PHY_BIN) ; then exit 1 ; fi 

ifneq ($(strip $(AEI_VDSL_CUSTOMER_NCS)),)
$(PHY_BIN2): $(PHYBLD_EXE2)
	$(PHYBLD_EXE2) $(PHY_BIN2)
	if test ! -f $(PHY_BIN2) ; then exit 1 ; fi 

$(PHY_BIN3): $(PHYBLD_EXE3)
	$(PHYBLD_EXE3) $(PHY_BIN3)
	if test ! -f $(PHY_BIN3) ; then exit 1 ; fi 
endif
 
$(PHYBLD_EXE): $(obj)/$(adslphy)/*.[ch] $(PHYBLD_C)
ifeq "$(ADSL_SELF_TEST)" "y"
	cd $(obj)/$(adslphy); \
	$(HOSTCC) $(HOST_EXTRACFLAGS) -o $@ $(obj)/AdslTestPhyBld.c $(obj)/$(adslphy)/adsl_selftest_lmem.c $(obj)/$(adslphy)/adsl_selftest_sdram.c; \
	cd $(obj)
else
	cd $(obj)/$(adslphy); \
	$(HOSTCC) $(HOST_EXTRACFLAGS) -o $@ $(obj)/AdslPhyBld.c $(obj)/$(adslphy)/adsl_lmem.c $(obj)/$(adslphy)/adsl_sdram.c; \
	cd $(obj)
endif	

ifneq ($(strip $(AEI_VDSL_CUSTOMER_NCS)),)
$(PHYBLD_EXE2): $(obj)/$(adslphy2)/*.[ch] $(PHYBLD_C)
	cd $(obj)/$(adslphy2); \
	$(HOSTCC) $(HOST_EXTRACFLAGS) -o $@ $(obj)/AdslPhyBld.c $(obj)/$(adslphy2)/adsl_lmem.c $(obj)/$(adslphy2)/adsl_sdram.c; \
	cd $(obj)

$(PHYBLD_EXE3): $(obj)/$(adslphy3)/*.[ch] $(PHYBLD_C)
	cd $(obj)/$(adslphy3); \
	$(HOSTCC) $(HOST_EXTRACFLAGS) -o $@ $(obj)/AdslPhyBld.c $(obj)/$(adslphy3)/adsl_lmem.c $(obj)/$(adslphy3)/adsl_sdram.c; \
	cd $(obj)
endif

ifneq ($(strip $(AEI_VDSL_CUSTOMER_NCS)),)
ifeq "$(BRCM_PHY_BONDING)" "y"
$(obj)/adsldrv.o: $(PHY_BIN) $(PHY_BIN2) $(PHY_BIN3)
targets += $(obj)/adsldrv.o
else
$(obj)/adsldrv.o: $(PHY_BIN)
targets += $(obj)/adsldrv.o
endif
else
$(obj)/adsldrv.o: $(PHY_BIN)
targets += $(obj)/adsldrv.o
endif
 
else

ifeq "$(ADSL_SELF_TEST)" "y"
adsldd-objs += $(adslphy)/adsl_selftest_lmem.o $(adslphy)/adsl_selftest_sdram.o
else
adsldd-objs += $(adslphy)/adsl_lmem.o $(adslphy)/adsl_sdram.o
endif
	
endif
	
-include $(TOPDIR)/Rules.make

