#!/bin/sh

# tftpsave?          "N" - Don't save via tftp
#                    "Y" - Save via tftp and remove tmp file
tftpsave="Y"
tftphost=""

#----------------------------------------------
if [ $# -eq 0 ]; then
    echo "Dumping memory information into /tmp/mdump.timestamp"
    echo "  '$0 tftpserver_ip' will copy to a server."
    tftpsave="N"
else
    echo "Dumping memory to tftpserver $1:/tmp/mdump.timestamp"
    tftphost=$1
fi

while [ 1 ]; do
    tmpfile="mdump.`date \"+%s\"`"

    cat /proc/*/stat > /tmp/$tmpfile
    cat /proc/meminfo >> /tmp/$tmpfile
    cat /proc/slabinfo >> /tmp/$tmpfile

    if [ "$tftpsave" = "Y" ]; then
	tftp -p -r $tmpfile -l /tmp/$tmpfile $tftphost
	rm -rf /tmp/$tmpfile
    fi
    sleep 1800 # Ever 30 minutes drop a sample
done
