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

#ifndef _MV_PHY_H
#define _MV_PHY_H

int     mv_phySetup(int unit);
int     mv_phyIsUp(int unit);
int     mv_phySpeed(int unit);
int     mv_phyIsFullDuplex(int unit);
int     mv_phyRegRead(int unit, int page, int reg, unsigned short *val);
int     mv_phyRegWrite(int unit, int page, int reg, unsigned short val);
int     mvPhy_ioctl(uint32_t *args, int cmd);

#endif	/* !_GEN_PHY_H */
