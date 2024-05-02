#!/bin/sh
#
# Start the xcloud....
#
/bin/mkdir -p /writable/cert/xcloud/
/bin/cp -f /opt/cmm/cert/xcloud/cacert.pem /writable/cert/xcloud/
/sbin/modprobe xt_tcpudp
/sbin/modprobe xt_TCPMSS
/usr/sbin/iptables -A INPUT -p tcp --tcp-flags SYN,RST SYN -j TCPMSS  --set-mss 1420
/usr/sbin/iptables -A OUTPUT -p tcp --tcp-flags SYN,RST SYN -j TCPMSS  --set-mss 1420
/usr/sbin/iptables -A INPUT -p tcp --dport 12224 -j DROP

exit 0

