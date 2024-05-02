/*
 * $Id: phy.h,v 1.12 2005/01/29 00:47:45 sdk Exp $
 * $Copyright:
 * All Rights Reserved.$
 *
 * Useful constants and routines for PHY chips which
 * run with 10/100/1000 Mb/s enet and use MII, GMII.
 *
 * See Also: IEEE 802.3 (1998) Local and Metropolitan Area Networks
 * Sections 22.2.4-XX
 */

#ifndef _LANTIQ_PHY_H
#define _LANTIQ_PHY_H

#if 0
int     lantiq_phySetup(int unit);
int     lantiq_phyIsUp(int unit);
int     lantiq_phySpeed(int unit);
int     lantiq_phyIsFullDuplex(int unit);
int     lantiq_phyRegRead(int unit, int page, int reg, unsigned short *val);
int     lantiq_phyRegWrite(int unit, int page, int reg, unsigned short val);
int     lantiqPhy_ioctl(uint32_t *args, int cmd);
#else
int lantiq_register_ops(void *arg);
#endif

#endif	/* !_LANTIQ_PHY_H */
