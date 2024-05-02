#!/bin/sh
#
# Start/stop dnsmasq
#       caching DNS server and DHCP server
#
_cmd=dnsmasq
_pid=/var/run/${_cmd}.pid
_dir=/etc/dnsmasq
_params=${_dir}/dhcpd.params
_listen=${_dir}/dns.listen

_start()
{
    dns_only=$1
    rc=
    lopts=
    dopts=
    if [ -f ${_listen} ] ; then
        read ad br < ${_listen}
        if [ "${ad}" != "" ] ; then
            lopts="-a ${ad}"
        fi
    fi
    if [ "${dns_only}" = "dns_only" ]; then
        # only want dns functionality
        # $1 == dns_only, need to shift
        shift
        dopts=
    else
        if [ -f ${_params} ] ; then
            read -r dopts < ${_params}
        fi
    fi

    if [ "${lopts}" != "" -o "${dopts}" != "" ] ; then
        echo "Starting ${_cmd} ..."
        _cmd="/usr/sbin/${_cmd} ${lopts} ${dopts} $*"
        _cmd="${_cmd} > /tmp/dnsmasq/dnslog.txt 2>&1"
        sleep 1
        eval ${_cmd}
        rc=$?
    fi
    return ${rc}
}

_stop()
{
    echo -n "Stopping ${_cmd} ..."
    if [ -f ${_pid} ] ; then
        read oldpid < ${_pid}
        if [ ${oldpid} ] ; then
            kill ${oldpid}
        fi
        rm -f ${_pid}
    else
        killall dnsmasq 2>/dev/null
    fi
    echo
}

_restart() {
    _stop
    sleep 1
    _start $*
}

op="$1" ; shift
case "${op}" in
start)
    _start $*
    ;;
stop)
    _stop
    ;;
stop_dhcp)
    # only stop the dhcp server
    _stop
    _start dns_only
    ;;
restart)
    _restart $*
    ;;
*)
    echo $"Usage: $0 {start|stop|restart}"
    exit 1
esac

exit $?
