#ifndef _RTL8363_PHY_H
#define _RTL8363_PHY_H

int     rtl8363_phySetup(int unit);
int     rtl8363_phyIsUp(int unit);
int     rtl8363_phySpeed(int unit);
int     rtl8363_phyIsFullDuplex(int unit);
uint32_t    smiRead(uint32_t phyad, uint32_t regad, uint32_t *data);
uint32_t    smiWrite(uint32_t phyad, uint32_t regad, uint32_t data);
int     rtl8363_ioctl(uint32_t *args, int cmd);

#endif
