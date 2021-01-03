# linupnp makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -Iinclude
CFLAGS += -I../../include
CFLAGS += -I../../include/bcmcrypto
CFLAGS += -I../../shared
CFLAGS += -I../../router/shared
CFLAGS += -DDSLCPE
CFLAGS += -Wall
CFLAGS += -DNDEBUG

#CFLAGS += -g -DDEBUG -UNDEBUG -DBCMDBG


LDFLAGS = -Os
LDFLAGS += -L$(EXTRALIBDIR)
LDFLAGS += -lc
LDFLAGS += -lgcc_s
LDFLAGS += -L$(INSTALL_DIR)/lib
LDFLAGS += -lnvram
LDFLAGS += -lwlbcmcrypto
LDFLAGS += -lwlbcmshared

SRCFILES = upnp.c upnp_ssdp.c upnp_http.c upnp_gena.c upnp_soap.c \
	upnp_description.c upnp_device.c upnp_util.c upnp_msg.c \
	upnp_linux_osl.c

OBJS = $(foreach x, $(SRCFILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

$(warning $(OBJS))

vpath %.c ./upnp
vpath %.c ./linux

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

libwlupnp.so: $(OBJS)
ifneq ($(strip $(OBJS)),)
	$(CC) -shared -Os -o $@ $^
endif
	install -m 755 libwlupnp.so $(INSTALL_DIR)/lib

clean:
	rm -f $(OBJS)
ifneq ($(strip $(OBJS)),)
	rm -f liwlbupnp.so
endif

dynamic static: libwlupnp.so
