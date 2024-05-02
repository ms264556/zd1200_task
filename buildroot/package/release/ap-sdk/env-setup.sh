#!/bin/bash

TOOLCHAINDIR="`pwd`/tools/toolchain"
BUILD_DIR="`pwd`/build"

set_env() 
{
    export PATH=${TOOLCHAINDIR}/bin:${PATH}
    echo "=== Set environment done ==="
}


if [ ! -d "${TOOLCHAINDIR}" ]; then
    echo "*** You need to install toolchain to /opt/2.6.15_gcc4.2.4 ***"
    echo "*** Step 1 - sudo mkdir /opt/2.6.15_gcc4.2.4 ***"
    echo "*** Step 2 - extract `pwd`/tools/rks_ap_sdk_toolchain-1.0.tar.bz2 ***"
    echo "sudo tar jxf `pwd`/tools/rks_ap_sdk_toolchain-1.0.tar.bz2 -C ${TOOLCHAINDIR}"
else
    set_env
    cd ${BUILD_DIR}; make help
fi
