# tpm settings
TPM=`cat /proc/v54bsp/tpm`
if [ $TPM -eq 1 ] ; then
insmod /lib/modules/2.6.32.24/kernel/drivers/char/tpm/tpm.ko
insmod /lib/modules/2.6.32.24/kernel/drivers/char/tpm/tpm_i2c_infineon.ko
mkdir -p /writable/data/tpm
chmod 0700 /writable/data/tpm
if [ ! -f /writable/data/tpm/tcsd.conf ] ; then
    cp -f /mnt/tpm/tcsd.conf /writable/data/tpm/
fi
chmod 0600 /writable/data/tpm/tcsd.conf
if [ ! -f /writable/data/tpm/system.data ] ; then
    cp -f /mnt/tpm/system.data /writable/data/tpm/
fi
chmod 0600 /writable/data/tpm/system.data
/usr/sbin/tcsd
else
echo "No TPM Device"
fi
