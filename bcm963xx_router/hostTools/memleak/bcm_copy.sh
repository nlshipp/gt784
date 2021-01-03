SRC=$1
DST=$2

cp $SRC/userspace/private/apps/smd/smd \
$SRC/userspace/private/apps/ssk/ssk \
$SRC/userspace/private/apps/sntp/sntp \
$SRC/userspace/private/apps/wlan/wlmngr/wlmngr \
$SRC/userspace/public/apps/ppp/pppoe/pppd \
$SRC/userspace/private/apps/tr64/tr64c \
$SRC/userspace/private/apps/tr69c_src/tr69c \
$SRC/userspace/private/apps/httpd/httpd \
$SRC/userspace/private/libs/cms_core/libcms_core.so \
$SRC/userspace/public/libs/cms_util/libcms_util.so \
$SRC/userspace/private/libs/cms_dal/libcms_dal.so \
$SRC/userspace/public/libs/cms_msg/libcms_msg.so \
$SRC/userspace/public/libs/cms_boardctl/libcms_boardctl.so \
$SRC/userspace/private/libs/xdslctl/libxdslctl.so \
$SRC/userspace/private/libs/nanoxml/libnanoxml.so \
$SRC/userspace/private/libs/atmctl/libatmctl.so \
$SRC/userspace/private/apps/wlan/wlmngr/libwlmngr.so \
$SRC/bcmdrivers/broadcom/net/wl/impl801/wl/exe/libwlctl.so \
$SRC/userspace/private/apps/wlan/nvram/libnvram.so \
$DST/ -f

cp $SRC/userspace/gpl/apps/udhcp/udhcpd \
$DST/dhcpc -f

cp $SRC/userspace/gpl/apps/udhcp/udhcpd \
$DST/dhcpd -f

