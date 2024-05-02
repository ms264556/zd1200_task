#!/bin/ash
#
#  Base on infomation from openvpn.org
#  kliou@ruckuswireless.com
#

let i=1
let j=1
unset fopt
unset dns dns0 dns1 dns2 dns3 dns4 dns5 dns6
unset opt
unset

# Convert ENVs to an array

while true ; do 
{
	eval fopt=\$foreign_option_$i
	if [ -z "${fopt}" ]; then
		break
	fi
	opt=${fopt}
	case $opt in
		*DOMAIN* ) domain=`eval echo ${opt} | \
				sed -e 's/dhcp-option DOMAIN //g'` ;;
		*DNS*    ) eval dns$j=`echo ${opt} | \
				sed -e 's/dhcp-option DNS //g'`
			       let j++ ;;
	esac
	let i++
}
done

# Now, do the work
tmpfile=/tmp/resolv.conf.tmp
cp /etc/resolv.conf ${tmpfile}

case "$script_type" in 
    up)
	let i=1
	while [ $i -lt $j ] ; do
		eval mydns="\$dns$i"
		sed -i -e "1 i nameserver ${mydns}" ${tmpfile} || exit 2
		let i++
	done

	if [ -n "${domain}" ]; then
		sed -i -e "$j i search ${domain}" ${tmpfile} || exit 3
	fi

	;;

    down)
	let i=1
	while [ $i -lt $j ] ; do
		eval mydns="\$dns$i"
		sed -i -e "/nameserver ${mydns}/D" ${tmpfile} || exit 4
		let i++
	done

	if [ -n "${domain}" ]; then
		sed -i -e "/search ${domain}/D" ${tmpfile} || exit 5
	fi
	
	;;

    *)
	echo "$0: invalid script_type" && exit 1 ;;
esac

cp ${tmpfile} /etc/resolv.conf
rm ${tmpfile}


# setup nat
nat_rules="POSTROUTING -o tun0 -j MASQUERADE"
iptables -t nat -D ${nat_rules} 2> /dev/null
iptables -t nat -A ${nat_rules}
echo -n 1 > /proc/sys/net/ipv4/ip_forward


# all done ...

exit 0
