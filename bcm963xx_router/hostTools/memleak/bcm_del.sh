DST=`pwd`

rm -f $DST/smd \
$DST/ssk \
$DST/sntp \
$DST/wlmngr \
$DST/pppd \
$DST/tr64c \
$DST/tr69c \
$DST/dslstatsd \
$DST/httpd \
$DST/libcms_core.so \
$DST/libcms_util.so \
$DST/libcms_dal.so \
$DST/libcms_msg.so \
$DST/libcms_boardctl.so \
$DST/libxdslctl.so \
$DST/libnanoxml.so \
$DST/libatmctl.so \
$DST/libwlmngr.so \
$DST/libwlctl.so \
$DST/libnvram.so \
$DST/dhcpc \
$DST/dhcpd \
$DST/memleak.log

if [ -f $DST/memory.bak.c -a "$1" != "" ]; then
mv $DST/memory.bak.c $1/memory.c
fi
