/*
 * $Id: phy.h,v 1.12 2005/01/29 00:47:45 sdk Exp $
 * $Copyright: (c) 2005 Broadcom Corp.
 * All Rights Reserved.$
 *
 * Useful constants and routines for PHY chips which
 * run with Orion (10/100/1000 Mb/s enet) and use MII, GMII,
 * or 8b/10b Serdes interfaces.
 *
 * See Also: IEEE 802.3 (1998) Local and Metropolitan Area Networks
 * Sections 22.2.4-XX
 */

#ifndef _BCM_PHY_H
#define _BCM_PHY_H

int     bcm_phySetup(int unit);
int     bcm_phyIsUp(int unit);
int     bcm_phySpeed(int unit);
int     bcm_phyIsFullDuplex(int unit);
unsigned int    bcm_phyID(int unit);
int     bcm_stats(int unit, uint8_t **stats);
void    bcm_reset_stats(int unit);
void    bcm_port_protect(int unit, int on);
void    bcm_port_vlan(int unit, int port, uint16_t mask);
void    bcm_disable_learn(int unit, uint16_t mask);

#define LAST_MIB_REGS                   0xbc

#endif	/* !_BCM_PHY_H */
