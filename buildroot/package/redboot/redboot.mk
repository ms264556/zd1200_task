#############################################################
#
# redboot
#
#############################################################

# default for GD6 builds
BR2_PACKAGE_REDBOOT_PATH ?= ../atheros/linux/ap/src/redboot_cobra

REDBOOT_SRC_PATH = $(PWD)/$(strip $(subst ",, $(BR2_PACKAGE_REDBOOT_PATH)))

# GPL tarball: manually prepare and put this file under buildroot/dl
REDBOOT_GPL_SRC  = redboot_cobra.tar.gz

BR2_PACKAGE_BSP_PATH ?= ../atheros/linux/driver/v54bsp
RB_BSP_PATH := $(BASE_DIR)/$(strip $(subst ",, $(BR2_PACKAGE_BSP_PATH)))
V54_INCS = -I$(V54_DIR)/linux -I$(V54_DIR)/apps/rfw


ifeq ($(strip $(BR2_RKS_BOARD_GD4)),y)
REDBOOT_ATH_BOARD_TYPE=ap43
REDBOOT_ATH_ENET_PHY=marvell
else
REDBOOT_ATH_BOARD_TYPE=ap51
REDBOOT_ATH_ENET_PHY=marvell
endif


ifeq ($(strip $(BR2_RKS_BOARD_RETRIEVER)),y)
REDBOOT_ATH_BOARD_TYPE=pb42
REDBOOT_ATH_ENET_PHY=broadcom
endif

ifeq ($(strip $(BR2_RKS_BOARD_DF1)),y) 
REDBOOT_ATH_BOARD_TYPE=pb42
REDBOOT_ATH_ENET_PHY=broadcom
RB_BSP_PATH := $(BASE_DIR)/../atheros/linux/ap71/src/drivers/v54bsp
endif


V54_INCS += -I$(RB_BSP_PATH)

BSP_OPTS := V54BSP_PATH=$(RB_BSP_PATH)
V54_OPTS := V54_INCS="$(V54_INCS)" $(BSP_OPTS)

ifdef RD_DRAM

REDBOOT_ATH_DRAM_SIZE=$(RD_DRAM)

else

REDBOOT_ATH_DRAM_SIZE=8
ifeq ($(strip $(BR2_TARGET_DRAM_32MB)),y)
REDBOOT_ATH_DRAM_SIZE=32
else
ifeq ($(strip $(BR2_TARGET_DRAM_16MB)),y)
REDBOOT_ATH_DRAM_SIZE=16
else
ifeq ($(strip $(BR2_TARGET_DRAM_64MB)),y)
REDBOOT_ATH_DRAM_SIZE=64
endif
endif
endif

endif

REDBOOT_ATH_FLASH_MB=2
ifeq ($(strip $(BR2_TARGET_FLASH_4MB)),y)
REDBOOT_ATH_FLASH_MB=4
else
ifeq ($(strip $(BR2_TARGET_FLASH_8MB)),y)
REDBOOT_ATH_FLASH_MB=8
else
ifeq ($(strip $(BR2_TARGET_FLASH_16MB)),y)
REDBOOT_ATH_FLASH_MB=16
endif
endif
endif

ifeq ($(strip $(BR2_REDBOOT_CLEAN_BUILD)),y)
MK_OPTS = REDBOOT_CLEAN_BUILD=y
else
MK_OPTS = 
endif


redboot_info:
	@echo "Making Redboot"

redboot:
	-mkdir /tftpboot/redboot
	make -C $(REDBOOT_SRC_PATH) ATH_ROOT=$(REDBOOT_SRC_PATH) ATH_INSTALL=$(BUILD_DIR) AP_TYPE=$(REDBOOT_ATH_BOARD_TYPE) FLASH_MB=$(REDBOOT_ATH_FLASH_MB) ENET_PHY=$(REDBOOT_ATH_ENET_PHY) DRAM_MB=$(REDBOOT_ATH_DRAM_SIZE) $(V54_OPTS) $(MK_OPTS) $(REDBOOT_ATH_BOARD_TYPE)

ifeq ($(strip $(BR2_PACKAGE_REDBOOT)),y)
HOWTO = redboot-howto
else
HOWTO = 
endif

REDBOOT_PREFIX = $(REDBOOT_ATH_BOARD_TYPE).$(REDBOOT_ATH_DRAM_SIZE)
REDBOOT_ROM = $(REDBOOT_PREFIX).rom
REDBOOT_RAM = $(REDBOOT_PREFIX).ram
REDBOOT_BIN = $(REDBOOT_PREFIX).bin
BDI_FTP_DIR = //tioga/ftp/redboot

redboot-flash-info : $(HOWTO)

redboot-howto:
	@echo "\nFlash redboot with following commands"
	@echo "Copy $(BUILD_DIR)/redboot/$(REDBOOT_ATH_BOARD_TYPE)/rom/install/bin/$(REDBOOT_ROM) to tftpdir"
	@echo -e "\n  *** WARNING *** \n  Load the RedBoot RAM image to update RedBoot ROM"
	@echo -e "  Reference //depot/atheros/linux/ap/docs/InstallRedBoot.txt\n"
	@echo -e "If loading redboot for the first time"
	@echo -e "\tRedBoot> fis init -f"
	@echo -e "\nTo load redboot ..."
	@echo -e "\tRedBoot> ip_address -l <AP_ip_addr> -h <tftp_server_ip_addr>"
	@echo -e "\tRedBoot> load -r -b %{FREEMEMLO} $(REDBOOT_ROM)"
	@echo -e "\tRedBoot> fis create -l 0x30000 -e 0xbfc00000 RedBoot"
	@echo -e "\t\t--> answer y"
	@echo -e "\tRedBoot> reset\n"

redboot-bdi:
	@echo -e "\ncopy /tftpboot/$(REDBOOT_BIN) to $(BDI_FTP_DIR)"
	@echo -e "\nBDI command to load redboot"
	@echo -e "  Use BIN version of redboot for BDI"
	@echo -e "  at the BDI prompt, enter the commands:\n"
	@make -s -C $(BR2_PACKAGE_REDBOOT_PATH) ATH_INSTALL=$(BUILD_DIR) AP_TYPE=$(REDBOOT_ATH_BOARD_TYPE) FLASH_MB=$(REDBOOT_ATH_FLASH_MB) DRAM_MB=$(REDBOOT_ATH_DRAM_SIZE) $(REDBOOT_ATH_BOARD_TYPE)-info



redboot-flash: redboot redboot-howto

redboot-test:
	@echo "TOPDIR=$(TOPDIR)"
	@echo "PROFILE=$(PROFILE)"

redboot-clean: redboot-dirclean
	rm -f $(BUILD_DIR)/redboot.*

redboot-dirclean:
	rm -rf $(BUILD_DIR)/redboot


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_REDBOOT)),y)
TARGETS+=redboot
endif

