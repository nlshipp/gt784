#! /bin/sh

ifconfig br0 promisc up
ebtables -A INPUT -i eth0 -p ! 888e -j DROP
ebtables -A INPUT -i eth1 -p ! 888e -j DROP
ebtables -A INPUT -i eth2 -p ! 888e -j DROP
ebtables -A INPUT -i eth3 -p ! 888e -j DROP
hostapd /var/hostapd.conf -KKdt&

