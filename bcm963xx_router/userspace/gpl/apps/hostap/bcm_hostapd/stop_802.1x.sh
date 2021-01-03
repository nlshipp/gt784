#! /bin/sh

ifconfig br0 -promisc 
ebtables -F INPUT
killall -9 hostapd


