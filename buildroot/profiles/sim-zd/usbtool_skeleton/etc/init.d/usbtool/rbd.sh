#!/bin/sh

usage()
{

 echo "Usage: rbd.sh {board} {model} {serial} {OUI} {mac1} {mac2}" 
 echo "Example: rbd.sh shiba ZD1025 12345678 00:13:92 01:01:01 01:01:02"
 exit 0

}

if [ $# -ne 6 ] ; then
  usage
fi

if [ ! -d /proc/v54bsp ] ; then
    echo "[rbd.sh] BSP not found - $*]"
    exit 255
fi

BOARD_NAME=$1
MODEL=$2
SERIAL=$3
OUI=$4
MAC1=$5
MAC2=$6


rbd change > /dev/null <<EOF
$BOARD_NAME
$SERIAL

$MODEL
$BOARD_NAME
3




n
y

$OUI


$MAC1
$MAC2



















n
n
n
n
y
8
Y
EOF

echo ""
echo "Board Data Written:"
echo -n "Board   :" ; cat /proc/v54bsp/name
echo -n "Model   :" ; cat /proc/v54bsp/model
echo -n "Serial  :" ; cat /proc/v54bsp/serial
echo -n "MAC1    :" ; rbd dump | grep eth0 | cut -d ' ' -f 8
echo -n "MAC2    :" ; rbd dump | grep eth1 | cut -d ' ' -f 8
echo ""

echo "rbd:PASS"
