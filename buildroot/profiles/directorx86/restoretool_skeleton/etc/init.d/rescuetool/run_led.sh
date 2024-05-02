#!/bin/sh

i=0
while [ 1 ]; do
   if [ $i -eq 1 ]; then
      /bin/led.sh green
      i=0
   else
      /bin/led.sh red
      i=1
   fi
   sleep 1
done
