#! /bin/sh

#usage
if [ "$1" = "" ] || [  "$2" = "" ] || [  "$3" = "" ] ; then
    echo "*************************************************************"
    echo "please input radius's ip_address, port, secret_key like below"
    echo "$./config_hostap_radius.sh 192.168.1.2 1812 secret"
    echo "*************************************************************"
    exit 0
fi

cp /etc/hostapd_t.conf /var/hostapd.conf
echo "auth_server_addr=$1" >> /var/hostapd.conf
echo "auth_server_port=$2" >> /var/hostapd.conf
echo "auth_server_shared_secret=$3" >> /var/hostapd.conf
