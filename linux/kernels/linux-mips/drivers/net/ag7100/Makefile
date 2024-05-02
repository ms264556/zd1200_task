#
# Makefile for Ar7100 gigebit MAC and Phys
#
obj-$(CONFIG_AG7100)			+=	ag7100_mod.o

obj-phy-$(CONFIG_BROADCOM_AC201_PHY)	=	brcmPhy.o
obj-phy-$(CONFIG_MV_PHY)        =	mvPhy.o rtl8363Phy.o rtl8363_asicdrv.o brcmPhy.o athrs16_phy.o athrf1_phy.o ar7240_s26_phy.o lantiq_phy.o
obj-phy-$(CONFIG_BCM5397_PHY)	=	bcmPhy.o
obj-phy-$(CONFIG_VITESSE_8201_PHY)	=	vsc_phy.o
obj-phy-$(CONFIG_VITESSE_8601_PHY)	=	vsc8601_phy.o
obj-phy-$(CONFIG_VITESSE_8601_7395_PHY)	=	vsc8601_phy.o generic_i2c.o generic_spi.o vsc73xx.o
obj-phy-$(CONFIG_AG7100_ICPLUS_PHY)	=	ipPhy.o
obj-phy-$(CONFIG_AG7100_REALTEK_PHY)	=	rtPhy.o
obj-phy-$(CONFIG_ADM6996FC_PHY)	=	adm_phy.o
obj-phy-$(CONFIG_ATHR_PHY) 	=	athrf1_phy.o
obj-phy-$(CONFIG_ATHRS26_PHY) 	=	athrs26_phy.o
obj-phy-$(CONFIG_AG7100_GENERIC_PHY)    = generic_phy.o

ag7100_mod-objs			:=	$(obj-phy-y) ag7100.o
