#!/bin/sh
PIDFILE=/tmp/ethbus-mon-pid
if [ -f $PIDFILE ]; then
    #PID file does exist so killing previous process
    kill -9 $(cat $PIDFILE)
fi
echo $$ > $PIDFILE

#We maintain a max of 2 logs at any given time
rotate_log () {
  size=$(ls -l /tmp/ethlog | awk '{ print $5}')
  if [ $size -gt 100000 ]; then
    mv /tmp/ethlog /tmp/ethlog.1
  fi
}
ethlog=/tmp/ethlog
ethtmp=/tmp/ethtmp
flag_run=0
flag_reboot=0
INTVL=300

#kernel module for dumping Ethernet PHY details
phy_det () {
    for i in 0 1 2 3 4 5 6 7 8 9 a b c d e f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e; do
      echo "Detecting PHY:${i}"
      printk 1
      echo ffe24528 ${i}01 > /proc/v54bsp/reg_wr
      echo ffe24524 0 > /proc/v54bsp/reg_wr
      echo ffe24524 1 > /proc/v54bsp/reg_wr
      echo ffe24534 > /proc/v54bsp/reg_rd
      printk 7
      echo ffe24530 > /proc/v54bsp/reg_rd
    done
}

{
while [ 1 ]; do
    #We ensure that the AP has atleast reached the RUN state once after it has come up
    if echo "$(apmgrinfo -A | grep State)" | egrep "RUN|CONF|DISC" | grep -v SOLE; then
        if echo "$(apmgrinfo -A | grep State)" | grep "RUN" | grep -v SOLE; then
            if [ $flag_run -eq 0 ]; then
                echo "First RUN state" >> $ethlog
                flag_run=1
            fi
        fi
	date >> $ethlog
        echo "nothing" >> $ethlog
    #The script logic kicks in if the AP goes into SOLE RUN state after being in RUN state atleast once
    elif [ $flag_run -eq 1 ]; then
        date >> $ethlog
        apmgrinfo -a >> $ethlog
        ifconfig eth0 >> $ethlog
        ifconfig eth1 >> $ethlog
        #The ethernet interface are brought down momentarily to allow for the kernel module to dump the stats
        ifconfig eth0 down
        ifconfig eth1 down
        phy_det
        dmesg | grep ffe24530 | tail -n31 > $ethtmp
        if echo "$(sed -n 6p $ethtmp)" | grep "0000ffff"; then
            echo "port 5 gone bad" >> $ethlog
            flag_reboot=1
        fi
        if echo "$(sed -n 7p $ethtmp)" | grep "0000ffff"; then
            echo "port 6 gone bad" >> $ethlog
            flag_reboot=1
        fi
        cat $ethtmp >> $ethlog
        ifconfig eth0 up
        ifconfig eth1 up
        rotate_log
        #Only when the ports 5 and/or 6 are 0xFFFF for the 7982 board's Ethernet PHY chip we flag an issue
        if [ $flag_reboot -eq 1 ]; then
            echo "Reboot AP Ethernet PHY unresponsive" >> $ethlog
            #Since we lose the contents in /tmp we copy the contents to the flash in a zipped format
	    minigzip /tmp/ethlog*
            cp /tmp/ethlog* /writable/
	    echo " "$(date) "                              Ethernet PHY unresponsive" | cut -d' ' -f 1-5,7- >> /writable/etc/system/reboot_reason
            rkscli -c "reboot"
        #The AP could go into SOLE RUN due to heartbeat loss due to network issues (non-hardware issue)
	elif [ $flag_reboot -eq 0 ]; then
	    echo "AP in SOLE RUN due to Non hardware Issue" >> $ethlog
	    minigzip /tmp/ethlog*
	    cp /tmp/ethlog* /writable/
        fi
    fi
    sleep $INTVL
done
} >/dev/null 2>&1
