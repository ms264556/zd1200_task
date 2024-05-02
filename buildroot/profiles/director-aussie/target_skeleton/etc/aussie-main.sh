#!/bin/sh -x
#***********************************************************
#  Copyright (c) 2005-2006    AireSpider Networks Inc.
#  All rights reserved
#***********************************************************

. /etc/aussie-func.sh

action=$1
shift

case $action in
    start)
        start_aussie ;;
    *)
        echo "$0  <start>" ;;
esac

