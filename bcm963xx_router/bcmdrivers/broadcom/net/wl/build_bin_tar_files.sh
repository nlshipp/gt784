#!/bin/bash

#############################################################################
# Find the impl from user input.

if [ -z $1 ]; then
  echo "Which impl to build?  Example:" $0 "7"
  exit
else
  imp="impl"$1
fi

if ! [ -d $impl ]; then
  exit
fi

echo Tar release on $imp

#############################################################################
# Get epi version and router version

./make_version.sh $imp $2
. $imp/epivers
ver=$EPI_VERSION_STR

. $imp/rver
rver=$ROUTER_VERSION_STR

#############################################################################
# These files are to be excluded.

exclude=" \
  --wildcards \
  --wildcards-match-slash \
  --exclude=.*.db \
  --exclude=*.cmd \
  --exclude=*.contrib* \
  --exclude=*.db \
  --exclude=*.keep* \
  --exclude=*.merge \
  --exclude=*.tmp \
  --exclude=*build \
  --exclude=tags \
  --exclude=dongle \
  --exclude=$imp/shared/cfe \
  --exclude=$imp/shared/linux \
  --exclude=$imp/shared/nvram \
  --exclude=$imp/shared/zlib \
  --exclude=$imp/usbdev \
"

bin_found=no

for x in `find $imp -maxdepth 1 -name *.o_save`; do
  bin_found=yes
done

if [ $bin_found != yes ]; then
  exit
fi

echo "Tarball==========" wlan-bin-all-$ver.tgz
tar -zcvf wlan-bin-all-$ver.tgz makefile.wlan $imp $exclude

if  [ $imp == "impl6" ]; then
  echo Add Dongle files to $imp release
  gunzip wlan-bin-all-$ver.tgz
  tar --append \
      --file=wlan-bin-all-$ver.tar \
             $imp/usbdev/usbdl/bcmdl \
             $imp/usbdev/libusb-0.1.12/libusb-0.1.12.tgz \
             $imp/dongle/rte/wl/builds/4322-bmac/rom-ag/rtecdc.trx
  gzip wlan-bin-all-$ver.tar
  mv wlan-bin-all-$ver.tar.gz wlan-bin-all-$ver.tgz
fi

#############################################################################
# Put all files in router version directory

if [ ! -d wlan-router.$ROUTER_VERSION_STR/bin ]; then
  mkdir -p wlan-router.$ROUTER_VERSION_STR/bin
fi

mv *tgz wlan-router.$ROUTER_VERSION_STR/bin

# End of file
