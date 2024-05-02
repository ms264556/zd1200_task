#############################################################
#
# uboot
#
#############################################################

# default for Collie builds
BR2_PACKAGE_UBOOT_PATH ?= ../atheros/linux/boot/u-boot

UBOOT_SRC_PATH = $(PWD)/$(strip $(subst ",, $(BR2_PACKAGE_UBOOT_PATH)))

# GPL tarball: manually prepare and put this file under buildroot/dl
UBOOT_GPL_SRC  =

BR2_PACKAGE_BSP_PATH ?= ../atheros/linux/driver/v54bsp
UB_BSP_PATH := $(BASE_DIR)/$(strip $(subst ",, $(BR2_PACKAGE_BSP_PATH)))
V54_INCS = -I$(BASE_DIR)/../video54/linux -I$(BASE_DIR)/../video54/apps/rfw

ifeq ($(strip $(BR2_RKS_BOARD_GD28)),y)
V54_BOARD_TYPE = GD28
endif

ifeq ($(strip $(BR2_RKS_BOARD_GD34)),y)
V54_BOARD_TYPE = GD34
endif

ifeq ($(strip $(BR2_RKS_BOARD_GD35)),y)
V54_BOARD_TYPE = GD35
endif

ifeq ($(strip $(BR2_RKS_BOARD_GD43)),y)
V54_BOARD_TYPE = GD43
endif

V54_BOARD_TYPE ?= gd8

ifeq ($(V54_BOARD_TYPE),gd8)
UBOOT_ATH_BOARD_TYPE=ap83
UBOOT_ATH_ENET_PHY=broadcom
endif

ifeq ($(V54_BOARD_TYPE),gd11)
UBOOT_ATH_BOARD_TYPE=ap94
UBOOT_ATH_ENET_PHY=marvell
endif

ifeq ($(V54_BOARD_TYPE),gd12)
UBOOT_ATH_BOARD_TYPE=ap83
UBOOT_ATH_ENET_PHY=marvell
endif

ifeq ($(V54_BOARD_TYPE),gd17)
UBOOT_ATH_BOARD_TYPE=ap101
UBOOT_ATH_ENET_PHY=atheros
endif

ifeq ($(V54_BOARD_TYPE),gd22)
UBOOT_ATH_BOARD_TYPE=ap94
UBOOT_ATH_ENET_PHY=marvell
endif

ifeq ($(V54_BOARD_TYPE),walle)
UBOOT_ATH_BOARD_TYPE=ap91
UBOOT_ATH_ENET_PHY=atheros
endif

ifeq ($(V54_BOARD_TYPE),pb92)
UBOOT_ATH_BOARD_TYPE=pb9x
UBOOT_ATH_ENET_PHY=atheros
endif

ifeq ($(V54_BOARD_TYPE),gd26)
UBOOT_ATH_BOARD_TYPE=db12x
UBOOT_ATH_ENET_PHY=atheros
endif

ifeq ($(V54_BOARD_TYPE),gd33)
UBOOT_ATH_BOARD_TYPE=db12x
UBOOT_ATH_ENET_PHY=atheros
endif

ifeq ($(V54_BOARD_TYPE),gd36)
UBOOT_ATH_BOARD_TYPE=db12x
UBOOT_ATH_ENET_PHY=atheros
endif

ifeq ($(V54_BOARD_TYPE),walle2)
UBOOT_ATH_BOARD_TYPE=db12x
UBOOT_ATH_ENET_PHY=atheros
endif

ifeq ($(V54_BOARD_TYPE),gd42)
UBOOT_ATH_BOARD_TYPE=board955x
UBOOT_ATH_ENET_PHY=atheros
endif

#snoop dogg
ifeq ($(subst ",,$(V54_BOARD_TYPE)),GD28)
UBOOT_ATH_BOARD_TYPE=P10XX
UBOOT_ATH_ENET_PHY=atheros
SPI_HEADER_CFG_FILE=config_ddr2_p10xx_667M_snoopdogg.dat
#SPI_HEADER_CFG_FILE=config_ddr2_p10xx_533M.dat
DTS_FILE=p1024rdb_32b.dts
endif

#cyprus2
ifeq ($(subst ",,$(V54_BOARD_TYPE)),GD34)
UBOOT_ATH_BOARD_TYPE=P10XX
UBOOT_ATH_ENET_PHY=atheros
SPI_HEADER_CFG_FILE=config_ddr2_p10xx_667M_cyprus2.dat
#SPI_HEADER_CFG_FILE=config_ddr2_p10xx_533M.dat
DTS_FILE=p1024rdb_32b_cyprus2.dts
endif

#corfu
ifeq ($(subst ",,$(V54_BOARD_TYPE)),GD35)
UBOOT_ATH_BOARD_TYPE=P10XX
UBOOT_ATH_ENET_PHY=atheros
SPI_HEADER_CFG_FILE=config_ddr2_p10xx_667M_snoopdogg.dat
#SPI_HEADER_CFG_FILE=config_ddr2_p10xx_533M.dat
DTS_FILE=p1024rdb_32b.dts
endif

#ridgeback
ifeq ($(subst ",,$(V54_BOARD_TYPE)),GD43)
UBOOT_ATH_BOARD_TYPE=P10XX
UBOOT_ATH_ENET_PHY=atheros
SPI_HEADER_CFG_FILE=config_ddr2_p10xx_667M_ridgeback.dat
#SPI_HEADER_CFG_FILE=config_ddr2_p10xx_667M_snoopdogg.dat
#SPI_HEADER_CFG_FILE=config_ddr2_p10xx_533M.dat
DTS_FILE=p1024rdb_32b.dts
endif

ifeq ($(subst ",,$(V54_BOARD_TYPE)),P10XX)
UBOOT_ATH_BOARD_TYPE=P10XX
UBOOT_ATH_ENET_PHY=atheros
SPI_HEADER_CFG_FILE=config_ddr2_p10xx_667M.dat
DTS_FILE=p1024rdb_32b.dts
endif

ifeq ($(subst ",,$(V54_BOARD_TYPE)),P1020RDB)
UBOOT_ATH_BOARD_TYPE=P1020RDB
UBOOT_ATH_ENET_PHY=atheros
SPI_HEADER_CFG_FILE=config_ddr2_p1020rdb_533M.dat
DTS_FILE=p1020rdb.dts
endif

V54_INCS += -I$(UB_BSP_PATH)

BSP_OPTS := V54BSP_PATH=$(UB_BSP_PATH)
V54_OPTS := V54_INCS="$(V54_INCS)" $(BSP_OPTS)

ifdef RD_DRAM

UBOOT_ATH_DRAM_SIZE=$(RD_DRAM)

else

UBOOT_ATH_DRAM_SIZE=8
ifeq ($(strip $(BR2_TARGET_DRAM_32MB)),y)
UBOOT_ATH_DRAM_SIZE=32
else
ifeq ($(strip $(BR2_TARGET_DRAM_16MB)),y)
UBOOT_ATH_DRAM_SIZE=16
else
ifeq ($(strip $(BR2_TARGET_DRAM_64MB)),y)
UBOOT_ATH_DRAM_SIZE=64
endif
endif
endif

endif

UBOOT_ATH_FLASH_MB=2
ifeq ($(strip $(BR2_TARGET_FLASH_4MB)),y)
UBOOT_ATH_FLASH_MB=4
else
ifeq ($(strip $(BR2_TARGET_FLASH_8MB)),y)
UBOOT_ATH_FLASH_MB=8
else
ifeq ($(strip $(BR2_TARGET_FLASH_16MB)),y)
UBOOT_ATH_FLASH_MB=16
endif
endif
endif

ifeq ($(strip $(BR2_UBOOT_CLEAN_BUILD)),y)
MK_OPTS = UBOOT_CLEAN_BUILD=y
else
MK_OPTS = 
endif


uboot_info:
	@echo "Making uboot"

uboot:
	@echo "Making uboot"
	@echo "Karan"
	@echo $(UBOOT_SRC_PATH)
	@echo $(BR2_PACKAGE_UBOOT_PATH)

	-mkdir -p /tftpboot/uboot
ifneq ($(strip $(DTS_FILE)),)
	make -C $(UBOOT_SRC_PATH)/../dtc
	$(UBOOT_SRC_PATH)/../dtc/dtc -b 0 -R 1024 -I dts -O dtb -o $(UBOOT_SRC_PATH)/fdt.dtb $(UBOOT_SRC_PATH)/../../kernels/linux-2.6.32.24/arch/powerpc/boot/dts/$(DTS_FILE)
	make -C $(UBOOT_SRC_PATH)/../bin2c
	$(UBOOT_SRC_PATH)/../bin2c/bin2c $(UBOOT_SRC_PATH)/fdt.dtb $(UBOOT_SRC_PATH)/include/dtb dtb_file 
endif
	make -C $(UBOOT_SRC_PATH) mrproper $(UBOOT_ATH_BOARD_TYPE)_config_$(subst ",,$(V54_BOARD_TYPE)) CROSS_COMPILE=$(TARGET_CROSS) SPIFLASH=$(SPIFLASH)
	make -C $(UBOOT_SRC_PATH) ATH_ROOT=$(UBOOT_SRC_PATH) ATH_INSTALL=$(BUILD_DIR) AP_TYPE=$(UBOOT_ATH_BOARD_TYPE) FLASH_MB=$(UBOOT_ATH_FLASH_MB) ENET_PHY=$(UBOOT_ATH_ENET_PHY) DRAM_MB=$(UBOOT_ATH_DRAM_SIZE) $(V54_OPTS) $(MK_OPTS) $(UBOOT_ATH_BOARD_TYPE) CROSS_COMPILE=$(TARGET_CROSS)
ifneq ($(strip $(SPI_HEADER_CFG_FILE)),)
	make -C $(UBOOT_SRC_PATH)/../boot_format CROSS_COMPILE=
	$(UBOOT_SRC_PATH)/../boot_format/boot_format $(UBOOT_SRC_PATH)/../boot_format/$(SPI_HEADER_CFG_FILE) $(UBOOT_SRC_PATH)/u-boot.bin -spi $(UBOOT_SRC_PATH)/u-boot.spi
endif

ifeq ($(strip $(BR2_PACKAGE_UBOOT)),y)
HOWTO = uboot-howto
else
HOWTO = 
endif

#UBOOT_PREFIX = $(UBOOT_ATH_BOARD_TYPE).$(UBOOT_ATH_DRAM_SIZE)
#UBOOT_ROM = $(UBOOT_PREFIX).rom
#UBOOT_RAM = $(UBOOT_PREFIX).ram
#UBOOT_BIN = $(UBOOT_PREFIX).bin
#BDI_FTP_DIR = //sam/a/ftp_root/pub/bdi/

uboot-flash-info : $(HOWTO)

uboot-howto:
	@echo -e "\nFlash uboot with following commands"
	@echo -e "Copy u-boot.bin to tftpdir"
	@echo -e "\n  *** WARNING *** \n  Load the u-boot RAM image to update u-boot ROM"
	@echo -e "\nTo load u-boot ..."
	@echo -e "\tu-boot> setenv ipaddr <AP_ip_addr>"
	@echo -e "\tu-boot> setenv serverip <tftp_server_ip_addr>"
	@echo -e "\tu-boot> tftpboot 0x81000000 u-boot.bin"
	@echo -e "\tu-boot> erase 1:0-0"
	@echo -e "\tu-boot> cp.l 0x81000000 0xbf000000 0x10000"
	@echo -e "\tu-boot> reset\n"

#uboot-bdi:
#	@echo -e "\ncopy /tftpboot/u-boot.bin to $(BDI_FTP_DIR)"
#	@echo -e "\nBDI command to load uboot"
#	@echo -e "  Use BIN version of redboot for BDI"
#	@echo -e "  at the BDI prompt, enter the commands:\n"
#	@make -s -C $(BR2_PACKAGE_REDBOOT_PATH) ATH_INSTALL=$(BUILD_DIR) AP_TYPE=$(REDBOOT_ATH_BOARD_TYPE) FLASH_MB=$(REDBOOT_ATH_FLASH_MB) DRAM_MB=$(REDBOOT_ATH_DRAM_SIZE) $(REDBOOT_ATH_BOARD_TYPE)-info



uboot-flash: uboot uboot-howto

uboot-test:
	@echo "TOPDIR=$(TOPDIR)"
	@echo "PROFILE=$(PROFILE)"

uboot-clean: uboot-dirclean
	rm -f $(BUILD_DIR)/uboot.*

uboot-dirclean:
	rm -rf $(BUILD_DIR)/uboot


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_UBOOT)),y)
TARGETS+=uboot
endif

