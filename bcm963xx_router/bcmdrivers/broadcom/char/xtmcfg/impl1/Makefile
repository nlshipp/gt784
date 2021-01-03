# File: bcmdrivers/char/xtmcfg/bcm963xx/Makefile
#
# Makefile for the BCM63xx bcmxtmcfg driver.
#

obj-$(CONFIG_BCM_XTMCFG) += bcmxtmcfg.o
bcmxtmcfg-objs := xtmcfgdrv.o xtmcfgmain.o xtmprocessor.o xtminterface.o xtmconnection.o xtmoamhandler.o xtmoslinux.o xtmasmhandler.o

EXTRA_CFLAGS += -DCONFIG_$(BRCM_CHIP) -fno-exceptions -fno-rtti -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_ADSLDRV_PATH) -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD)
EXTRA_CFLAGS += -DDPHY_LOOPBACK

clean:
	rm -f core *.o *.a *.s

