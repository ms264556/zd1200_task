#
# Copyright (c) 2013 Qualcomm Atheros, Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

ifdef QCA8035
GMAC_QCA955x=1
ATH_GMAC_GE0_IS_CONNECTED=1
ATH_GMAC0_MII=ATHR_RGMII
ETH_CONFIG=_f1e
ATH_GMAC_LEN_PER_TX_DS=1536
ATH_GMAC0_TX_CNT=128
ATH_GMAC0_RX_CNT=224
ATH_GMAC0_RXFCTL=1
ATH_GMAC0_TXFCTL=1
ATH_GMAC_RX_TASKLET=1
ATH_GMAC_RX_PKT_CNT=160
endif

#ifdef QCA8337
GMAC_QCA955x=1
ETH_CONFIG=_s17
ATH_GMAC_GE0_IS_CONNECTED=1
ATH_GMAC_GE1_IS_CONNECTED=1
ATH_GMAC0_MII=ATHR_RGMII
ATH_GMAC1_MII=ATHR_SGMII
ATH_CFG_SGMII=1
ATH_CONFIG_SWAP_SGMII=0
ATH_S17_PHY0_WAN=1
ATH_SGMII_FORCED=1
ATH_S17_MAC0_SGMII=1
ATH_RGMII_CAL=1
ATH_GMAC_LEN_PER_TX_DS=1536
ATH_GMAC0_TX_CNT=256
ATH_GMAC0_RX_CNT=224
ATH_GMAC1_TX_CNT=256
ATH_GMAC1_RX_CNT=224
ATH_GMAC0_RXFCTL=1
ATH_GMAC0_TXFCTL=1
ATH_GMAC1_RXFCTL=1
ATH_GMAC1_TXFCTL=1
ATH_GMAC_RX_TASKLET=1
ATH_GMAC_RX_PKT_CNT=160
ATH_GMAC_HW_QOS=0
#endif

BUILD_ETH_OUTPUT=$(KERNEL_OBJ_PATH)/drivers/net/qca955x_s17

ifeq ($(GMAC_QCA955x),1)

MAC-OBJS += qca955x.o athrs_gmac_ctrl.o athrs_phy_ctrl.o

ifeq ($(ATH_CFG_SGMII), 1)
MAC-OBJS += athrs_sgmii.o
endif

ifeq ($(ETH_CONFIG),_vir)
PHY-OBJS += phys/athrs_vir_phy.o
endif

ifeq ($(ETH_CONFIG2),_ar8033)
PHY-OBJS += phys/athr_ar8033_phy.o
ATH_GMAC_HW_QOS=0
endif

ifeq ($(ETH_CONFIG2),_vir)
PHY-OBJS += phys/athrs_vir_phy.o
endif

ifeq ($(ETH_CONFIG),_s17)
PHY-OBJS += phys/athrs17_phy.o
endif

ifeq ($(ETH_CONFIG),_s17_hwaccel)
override ETH_CONFIG:=_s17
export ATH_GMAC_HW_NAT=1
PHY-OBJS += phys/athrs17_phy.o
PHY-OBJS += phys/ssdk_ks_km.a
SSDK_PRJ_PATH=$(ENETDIR)/s17_ssdk/board/ssdk_header
EXTRA_CFLAGS += -I$(SSDK_PRJ_PATH)/include \
                -I$(SSDK_PRJ_PATH)/include/init \
                -I$(SSDK_PRJ_PATH)/include/common \
                -I$(SSDK_PRJ_PATH)/include/fal \
                -I$(SSDK_PRJ_PATH)/include/hsl \
                -I$(SSDK_PRJ_PATH)/include/hsl/isis \
                -I$(SSDK_PRJ_PATH)/include/sal/os \
                -I$(SSDK_PRJ_PATH)/include/sal/sd \
                -I$(SSDK_PRJ_PATH)/include/sal/sd/linux/hydra_howl \
                -I$(SSDK_PRJ_PATH)/include/sal/sd/linux/uk_interface \
                -I$(SSDK_PRJ_PATH)/include/init
endif

ifeq ($(ETH_CONFIG2),_s17)
PHY-OBJS += phys/athrs17_phy.o
ifeq ($(HYBRID_VLAN_IGMP), 1)
PHY-OBJS += phys/ssdk_ks_km.a
SSDK_PRJ_PATH=$(ENETDIR)/s17_ssdk/board/ssdk_header
EXTRA_CFLAGS += -I$(SSDK_PRJ_PATH)/include \
                -I$(SSDK_PRJ_PATH)/include/init \
                -I$(SSDK_PRJ_PATH)/include/common \
                -I$(SSDK_PRJ_PATH)/include/fal \
                -I$(SSDK_PRJ_PATH)/include/hsl \
                -I$(SSDK_PRJ_PATH)/include/hsl/isis \
                -I$(SSDK_PRJ_PATH)/include/sal/os \
                -I$(SSDK_PRJ_PATH)/include/sal/sd \
                -I$(SSDK_PRJ_PATH)/include/sal/sd/linux/hydra_howl \
                -I$(SSDK_PRJ_PATH)/include/sal/sd/linux/uk_interface \
                -I$(SSDK_PRJ_PATH)/include/init
endif
endif

ifeq ($(ETH_CONFIG),_f1e)
PHY-OBJS += phys/athrf1_phy.o
ATH_GMAC_GE1_IS_CONNECTED=0
ATH_GMAC0_RGMII=1
endif



ifeq ($(ATH_GMAC_SOC_HW_NAT),1)
EXTRA_CFLAGS += -I ${ENETDIR}/include/hwaccels
ACCEL-OBJS += hwaccels/athrs_hw_accels.o hwaccels/athrs_gmac_nf_nat.o hwaccels/athrs_gmac_955x_hw_nat.o hwaccels/athrs_nat.o
endif

ifeq ($(ATH_GMAC_SOC_HW_ACL),1)
EXTRA_CFLAGS += -I ${ENETDIR}/include/hwaccels
ACCEL-OBJS += hwaccels/athrs_hw_accels.o hwaccels/athrs_gmac_nf_acl.o hwaccels/athrs_gmac_955x_hw_acl.o hwaccels/athrs_acl.o
endif
endif

ifeq ($(ATH_GMAC_HW_QOS),1)
ifneq ($(ETH_CONFIG),_s17)
ifneq ($(ETH_CONFIG2),_s17)
ACCEL-OBJS += athrs_qos.o vlan_igmp.o
endif
endif
endif

$(BUILD_ETH_OUTPUT)/include/config.h:
ifeq ($(GMAC_QCA955x),1)
	@[ ! -d $(BUILD_ETH_OUTPUT)/include ] && mkdir -p $(BUILD_ETH_OUTPUT)/include || exit 0
	@> $(BUILD_ETH_OUTPUT)/include/config.h
	@echo "/*" > $(BUILD_ETH_OUTPUT)/include/config.h
	@echo "* Automatically generated make config: don't edit" >> $(BUILD_ETH_OUTPUT)/include/config.h
	@echo "*/" >> $(BUILD_ETH_OUTPUT)/include/config.h

ifeq ($(ATH_GMAC_SUPPORT_DUAL_PHY),1)
	@ echo "#define CONFIG_ATHR_SUPPORT_DUAL_PHY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
ifeq ($(ETH_CONFIG2),_s17)
	@echo "#define ATHR_GMAC1_MII_PHY  ATHR_S17_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATHRS17_PHY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ETH_CONFIG2),_ar8033)
	@echo "#define ATHR_GMAC1_MII_PHY ATHR_AR8033_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATHRS_AR8033_PHY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
ifeq ($(ATH_MDC_GPIO),1)
	@echo "#define ATH_MDC_GPIO       1"      >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
endif # _ar8033
endif # ATH_GMAC_SUPPORT_DUAL_PHY

ifeq ($(ETH_CONFIG),_s17)
	@echo "#define ATHR_GMAC0_MII_PHY ATHR_S17_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
# V54_BSP
	@echo "#define ATHR_GMAC1_MII_PHY ATHR_S17_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
ifneq (,$(findstring $(BOARD_TYPE),ap132 ap136 ap135 ap135_wapi))
	@echo "#define ATHR_GMAC1_MII_PHY ATHR_S17_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifneq (,$(findstring $(BOARD_TYPE),board955x-offload-target))
ifneq (,$(findstring $(BUILD_CONFIG),_ap136 _ap132 _ap135))
	@echo "#define ATHR_GMAC1_MII_PHY ATHR_S17_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
endif

ifneq (,$(findstring $(BOARD_TYPE),board955x-offload-host))
ifneq (,$(findstring $(BUILD_CONFIG),_ap136 _ap135_bootrom_pcie))
	@echo "#define ATHR_GMAC1_MII_PHY ATHR_S17_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
endif

	@echo "#define CONFIG_ATHRS17_PHY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h

endif
ifeq ($(ETH_CONFIG),_vir)
	@echo "#define ATHR_GMAC0_MII_PHY ATHR_VIR_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATHRS_VIR_PHY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h

endif
ifeq ($(ETH_CONFIG),_f1e)
	@echo "#define ATHR_GMAC0_MII_PHY ATHR_F1E_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATHRF1_PHY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
ifeq ($(ETH_CONFIG2),_vir)
	@echo "#define ATHR_GMAC1_MII_PHY ATHR_VIR_PHY" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATHRS_VIR_PHY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
ifeq ($(ATH_CFG_SGMII), 1)
	@echo "#define CONFIG_ATHRS_GMAC_SGMII 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_SGMII_LINK_INTR),1)
	@echo "#define CONFIG_ATH_SGMII_LINK_INTR 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifneq ($(findstring ATHR_RGMII,$(ATH_GMAC0_MII)),)
	@echo "#define ATHR_GMAC0_MII ATHR_RGMII" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif
ifneq ($(findstring ATHR_SGMII,$(ATH_GMAC0_MII)),)
	@echo "#define ATHR_GMAC0_MII ATHR_SGMII" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifneq ($(findstring ATHR_MII,$(ATH_GMAC0_MII)),)
	@echo "#define ATHR_GMAC0_MII ATHR_MII" >> $(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define GMAC0_MII 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifneq ($(findstring ATHR_SGMII,$(ATH_GMAC1_MII)),)
	@echo "#define ATHR_GMAC1_MII ATHR_SGMII" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif
ifneq ($(findstring ATHR_RGMII,$(ATH_GMAC1_MII)),)
	@echo "#define ATHR_GMAC1_MII ATHR_RGMII" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif
ifneq ($(findstring ATHR_MII,$(ATH_GMAC1_MII)),)
	@echo "#define ATHR_GMAC1_MII ATHR_MII" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_GMAC_GE0_IS_CONNECTED),1)
	@echo "#define ATH_GE0_IS_CONNECTED 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif
ifeq ($(ATH_GMAC_GE1_IS_CONNECTED),1)
	@echo "#define ATH_GE1_IS_CONNECTED 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_SLAVE_CONNECTED),1)
	@echo "#define CONFIG_SCO_SLAVE_CONNECTED 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(S17_SWMAC6_CONNECTED),1)
	@echo "#define CONFIG_S17_SWMAC6_CONNECTED 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_SWITCH_ONLY_MODE),1)
	@echo "#define CONFIG_ATHR_SWITCH_ONLY_MODE 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_S17_PHY0_WAN),1)
	@echo "#define CONFIG_ATH_S17_WAN 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_S17_MAC0_SGMII),1)
	@echo "#define ATH_S17_MAC0_SGMII 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_RGMII_CAL),1)
	@echo "#define ATH_RGMII_CAL 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_GMAC_DESC_SRAM),1)
	@echo "#define CONFIG_ATHR_DESC_SRAM 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_SGMII_FORCED),1)
	@echo "#define ATH_SGMII_FORCED_MODE 1" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif
	@echo "#define CONFIG_ATH_LEN_PER_TX_DS $(ATH_GMAC_LEN_PER_TX_DS)" >> $(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATH_NUMBER_TX_PKTS  $(ATH_GMAC0_TX_CNT)" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATH_NUMBER_RX_PKTS   $(ATH_GMAC0_RX_CNT)" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATH_NUMBER_TX_PKTS_1 $(ATH_GMAC1_TX_CNT)" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATH_NUMBER_RX_PKTS_1 $(ATH_GMAC1_RX_CNT)" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATH_MAC_LOCATION $(ATH_GMAC_LOCATION)" >>$(BUILD_ETH_OUTPUT)/include/config.h
ifdef ATH_GMAC_TXQUEUELEN
	@echo "#define ATH_GMAC_TXQUEUELEN $$ATH_GMAC_TXQUEUELEN" >> $(BUILD_ETH_OUTPUT)/include/config.h
else
	# refer to ether_setup
	@echo "#define ATH_GMAC_TXQUEUELEN 1000" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif


ifeq ($(ATH_GMAC_HW_NAT),1)
ifeq ($(ETH_CONFIG),_s17)
	@echo "#define S17_FULL_FEATURE 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATHRS17_HNAT 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
else
	@echo "#define CONFIG_ATHRS_HW_NAT  1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
endif

ifeq ($(ATH_GMAC_SOC_HW_NAT),1)
	@echo "#define CONFIG_ATHRS_GMAC_HW_NAT 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_GMAC_SOC_HW_ACL),1)
	@echo "#define CONFIG_ATHRS_GMAC_HW_ACL 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(OFFLOAD_TGT_RECOVERY),1)
	@echo "#define OFFLOAD_TGT_RECOVERY 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifneq (,$(findstring $(BOARD_TYPE),board955x-offload-target board955x-offload-host))
	@echo "#define CONFIG_OFFLOAD_BOOTROM_BUILD  1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
endif #END QCA955x





ifeq ($(ATH_GMAC_HW_QOS),1)
	@echo "#define CONFIG_ATHRS_QOS 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ATH_GMAC_RX_TASKLET),1)
	@echo "#define CONFIG_ATHR_RX_TASK 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATHR_RX_PKTS_CNT $(ATH_GMAC_RX_PKT_CNT)" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifdef ATH_GMAC_TXQUEUELEN
	@echo "#define ATH_GMAC_TXQUEUELEN $$ATH_GMAC_TXQUEUELEN" >> $(BUILD_ETH_OUTPUT)/include/config.h
else
	# refer to ether_setup
	@echo "#define ATH_GMAC_TXQUEUELEN 1000" >> $(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(strip ${ATH_CAL_NAND_FLASH}), 1)
	@echo "#define ATH_CAL_NAND_FLASH 1"	    >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define ATH_CAL_NAND_PARTITION "\"$(strip ${ATH_CAL_NAND_PARTITION})\"	    >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(strip ${ATH_GMAC_RXTIMER}), 1)
	@echo "#define CONFIG_GMAC_RXTIMER 1"	    >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_GMAC_RXTIMER_FREQ $(ATH_GMAC_RXTIMER_FREQ)"	    >>$(BUILD_ETH_OUTPUT)/include/config.h
else
	@echo "#define CONFIG_GMAC_RXTIMER 0"	    >>$(BUILD_ETH_OUTPUT)/include/config.h
endif

ifeq ($(ETHDEBUG),1)
	@echo "#define ETHDEBUG_ENABLED $(ETHDEBUG)" >>$(BUILD_ETH_OUTPUT)/include/config.h
else
	@echo "#define ETHDEBUG_ENABLED 0" >>$(BUILD_ETH_OUTPUT)/include/config.h
endif
	@echo "#define V54_DBG 1" >>$(BUILD_ETH_OUTPUT)/include/config.h
	@echo "#define CONFIG_ATH_JUMBO" >>$(BUILD_ETH_OUTPUT)/include/config.h
