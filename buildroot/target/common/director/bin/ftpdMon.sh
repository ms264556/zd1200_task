#!/bin/sh 

while true
   do
   sleep 300
   ps | grep vsftpd | grep -v grep > /dev/null
   if [ $? -ne 0 ]; then
      echo "restart vsftp"
      /etc/init.d/vsftpd start
   fi
done

