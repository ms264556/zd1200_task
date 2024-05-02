#ifndef _AG7100_PHY_H
#define _AG7100_PHY_H

#define phy_reg_read        ag7100_mii_read
#define phy_reg_write       ag7100_mii_write

#ifndef CONFIG_ATHRS26_PHY
#define ag7100_phy_ioctl(unit, args)
#endif

#include "ag7100.h"

#if defined(CONFIG_VITESSE_8201_PHY) || defined(CONFIG_VITESSE_8601_PHY)

#include "vsc8601_phy.h"

static inline int
ag7100_phy_is_up(int unit)
{
    int link;
    int fdx;
    ag7100_phy_speed_t speed;

    vsc8601_phy_get_link_status(unit, &link, &fdx, &speed, 0);
    return link;
}

static inline int
ag7100_phy_setup(int unit)
{
  return vsc8601_phy_setup(unit);
}

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  return vsc8601_phy_get_link_status(unit, link, fdx, speed, 0);
}

static inline int
ag7100_print_link_status(int unit)
{
  if (0==unit)
    return vsc8601_phy_print_link_status(unit);

  return -1;
}

#elif defined(CONFIG_VITESSE_8601_7395_PHY)

#include "vsc8601_phy.h"
#include "vsc73xx.h"

static inline int
ag7100_phy_setup(int unit)
{
  if (0==unit) {
    return vsc8601_phy_setup(unit);
  } else {
    if (1 == unit) {
      return vsc73xx_setup(unit);
    }
  }
  return -1;
}

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  if (0==unit)
    return vsc8601_phy_get_link_status(unit, link, fdx, speed, 0);
  else
    if (0 == in_interrupt())
      return vsc73xx_get_link_status(unit, link, fdx, speed, 0);

  return -1;
}

static inline int
ag7100_print_link_status(int unit)
{
  if (0==unit)
    return vsc8601_phy_print_link_status(unit);
  else
    if (0 == in_interrupt())
      return vsc73xx_phy_print_link_status(unit);
  return -1;
}

#elif defined(CONFIG_AG7100_ICPLUS_PHY)

#include "ipPhy.h"

#define ag7100_phy_setup(unit)          ip_phySetup(unit)
#define ag7100_phy_is_up(unit)          ip_phyIsUp(unit)
#define ag7100_phy_speed(unit)          ip_phySpeed(unit)
#define ag7100_phy_is_fdx(unit)         ip_phyIsFullDuplex(unit)

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link=ag7100_phy_is_up(unit);
  *fdx=ag7100_phy_is_fdx(unit);
  *speed=ag7100_phy_speed(unit);
  return 0;
}

static inline int
ag7100_print_link_status(int unit)
{
  return -1;
}

#elif defined(CONFIG_AG7100_REALTEK_PHY)

#include "rtPhy.h"

#define ag7100_phy_setup(unit)          rt_phySetup(unit, 0)
#define ag7100_phy_is_up(unit)          rt_phyIsUp(unit)
#define ag7100_phy_speed(unit)          rt_phySpeed(unit)
#define ag7100_phy_is_fdx(unit)         rt_phyIsFullDuplex(unit)

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link=ag7100_phy_is_up(unit);
  *fdx=ag7100_phy_is_fdx(unit);
  *speed=ag7100_phy_speed(unit);
  return 0;
}

static inline int
ag7100_print_link_status(int unit)
{
  return -1;
}

#elif defined(CONFIG_ADM6996FC_PHY)

#include "adm_phy.h"

#define ag7100_phy_setup(unit)          adm_phySetup(unit)
#define ag7100_phy_is_up(unit)          adm_phyIsUp(unit)
#define ag7100_phy_speed(unit)          adm_phySpeed(unit)
#define ag7100_phy_is_fdx(unit)         adm_phyIsFullDuplex(unit)
#define ag7100_phy_is_lan_pkt           adm_is_lan_pkt
#define ag7100_phy_set_pkt_port         adm_set_pkt_port
#define ag7100_phy_tag_len              ADM_VLAN_TAG_SIZE
#define ag7100_phy_get_counters         adm_get_counters

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link=ag7100_phy_is_up(unit);
  *fdx=ag7100_phy_is_fdx(unit);
  *speed=ag7100_phy_speed(unit);
  return 0;
}

static inline int
ag7100_print_link_status(int unit)
{
  return -1;
}

#elif defined(CONFIG_ATHRS26_PHY)

#include "athrs26_phy.h"

#define ag7100_phy_ioctl(unit, args)    athr_ioctl(unit,args)
#define ag7100_phy_setup(unit)          athrs26_phy_setup (unit)
#define ag7100_phy_is_up(unit)          athrs26_phy_is_up (unit)
#define ag7100_phy_speed(unit)          athrs26_phy_speed (unit)
#define ag7100_phy_is_fdx(unit)         athrs26_phy_is_fdx (unit)
#define ag7100_phy_is_lan_pkt           athr_is_lan_pkt
#define ag7100_phy_set_pkt_port         athr_set_pkt_port
#define ag7100_phy_tag_len              ATHR_VLAN_TAG_SIZE
#define ag7100_phy_get_counters         athrs26_get_counters

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link=ag7100_phy_is_up(unit);
  *fdx=ag7100_phy_is_fdx(unit);
  *speed=ag7100_phy_speed(unit);
  return 0;
}

static inline int
ag7100_print_link_status(int unit)
{
  return -1;
}

#elif defined(CONFIG_ATHRF1_PHY)

#include "athr_phy.h"

#define ag7100_phy_setup(unit)          athr_phy_setup(unit)
#define ag7100_phy_is_up(unit)          athr_phy_is_up(unit)
#define ag7100_phy_speed(unit)          athr_phy_speed(unit)
#define ag7100_phy_is_fdx(unit)         athr_phy_is_fdx(unit)
#define ag7100_phy_is_lan_pkt           athr_is_lan_pkt
#define ag7100_phy_set_pkt_port         athr_set_pkt_port
#define ag7100_phy_tag_len              ATHR_VLAN_TAG_SIZE
#define ag7100_phy_get_counters         athr_get_counters

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link=ag7100_phy_is_up(unit);
  *fdx=ag7100_phy_is_fdx(unit);
  *speed=ag7100_phy_speed(unit);
  return 0;
}

static inline int
ag7100_print_link_status(int unit)
{
  return -1;
}

#elif defined(CONFIG_MV_PHY)

#include "athr_phy.h"

static inline unsigned int
ag7100_get_link_status_athrf1(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link     = athr_phy_is_up(unit);
  *fdx      = athr_phy_is_fdx(unit);
  *speed    = athr_phy_speed(unit);
  return 0;
}

#include "lantiq_phy.h"

static inline unsigned int
ag7100_get_link_status_lantiq(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link     = lantiq_phyIsUp(unit);
  *fdx      = lantiq_phyIsFullDuplex(unit);
  *speed    = lantiq_phySpeed(unit);
  return 0;
}

#include "mvPhy.h"

static inline unsigned int
ag7100_get_link_status_mv(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link     = mv_phyIsUp(unit);
  *fdx      = mv_phyIsFullDuplex(unit);
  *speed    = mv_phySpeed(unit);
  return 0;
}

#include "rtl8363Phy.h"

static inline unsigned int
ag7100_get_link_status_rtl(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link     = rtl8363_phyIsUp(unit);
  *fdx      = rtl8363_phyIsFullDuplex(unit);
  *speed    = rtl8363_phySpeed(unit);
  return 0;
}

#include "athrs16_phy.h"

static inline unsigned int
ag7100_get_link_status_s16(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link     = athrs16_phy_is_up(unit);
  *fdx      = athrs16_phy_is_fdx(unit);
  *speed    = athrs16_phy_speed(unit);
  return 0;
}

#include "ar7240_s26_phy.h"

static inline unsigned int
ag7100_get_link_status_s26(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed, int phyUnit)
{
  *link=athrs26_phy_is_up(unit);
  *fdx=athrs26_phy_is_fdx(unit, phyUnit);
  *speed=athrs26_phy_speed(unit, phyUnit);
  return 0;
}

#include "brcmPhy.h"

static inline unsigned int
ag7100_get_link_status_brcm(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link     = bcm_ethIsLinkUp(unit);
  *fdx      = bcm_phyIsFullDuplex(unit);
  *speed    = bcm_phyIsSpeed100(unit);
  return 0;
}

static inline int
ag7100_print_link_status(int unit)
{
  return -1;
}

#elif defined(CONFIG_BROADCOM_AC201_PHY)

#include "brcmPhy.h"

#define ag7100_phy_setup(unit)          bcm_phySetup(unit, 0)
#define ag7100_phy_is_up(unit)          bcm_ethIsLinkUp(unit)
#define ag7100_phy_speed(unit)          bcm_phyIsSpeed100(unit)
#define ag7100_phy_is_fdx(unit)         bcm_phyIsFullDuplex(unit)

static inline unsigned int
ag7100_get_link_status(int unit, int *link, int *fdx, ag7100_phy_speed_t *speed)
{
  *link=ag7100_phy_is_up(unit);
  *fdx=ag7100_phy_is_fdx(unit);
  *speed=ag7100_phy_speed(unit);
  return 0;
}

static inline int
ag7100_print_link_status(int unit)
{
  return -1;
}

#else
#error unknown PHY type PHY not configured in config.h
#endif

#endif
