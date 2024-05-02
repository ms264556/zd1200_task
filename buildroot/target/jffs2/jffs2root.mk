#############################################################
#
# Build the jffs2 root filesystem image
#
#############################################################

EBSIZE := $(strip $(subst ",,$(BR2_TARGET_JFFS2_EBSIZE)))
ifeq ($(EBSIZE),)
JFFS2_OPTS := -e 0x10000
else
JFFS2_OPTS := -e $(strip $(BR2_TARGET_JFFS2_EBSIZE))
endif

EBSIZE_16M := $(strip $(subst ",,$(BR2_TARGET_JFFS2_EBSIZE_16M)))
ifeq ($(EBSIZE_16M),)
JFFS2_OPTS_16M := -e 0x40000
else
JFFS2_OPTS_16M := -e $(strip $(BR2_TARGET_JFFS2_EBSIZE_16M))
endif

JFFS2_PAD=
ifeq ($(strip $(BR2_TARGET_ROOTFS_JFFS2_PAD)),y)
JFFS2_PAD += -p
ifneq ($(strip $(BR2_TARGET_ROOTFS_JFFS2_PADSIZE)),0x0)
JFFS2_PAD += $(strip $(BR2_TARGET_ROOTFS_JFFS2_PADSIZE))
endif
endif

ifeq ($(strip $(BR2_TARGET_ROOTFS_JFFS2_SQUASH)),y)
JFFS2_OPTS += -q
endif

ifeq ($(strip $(BR2_TARGET_JFFS2_LE)),y)
JFFS2_OPTS += -l
JFFS2_OPTS_16M += -l
endif

ifeq ($(strip $(BR2_TARGET_JFFS2_BE)),y)
JFFS2_OPTS += -b
JFFS2_OPTS_16M += -b
endif

#JFFS2_DEVFILE = $(strip $(subst ",,$(BR2_TARGET_ROOTFS_JFFS2_DEVFILE)))
#ifneq ($(strip $(TARGET_DEVICE_TABLE)) ,)
#JFFS2_OPTS += -D $(TARGET_DEVICE_TABLE)
#endif

JFFS2_TARGET := $(subst ",,$(BR2_TARGET_ROOTFS_JFFS2_OUTPUT))

jffs_info : 
	@echo "OPTS = $(JFFS2_OPTS)"
	@echo "ROOTFS_PAD = $(JFFS2_PAD)"
	@echo "DATAFS_PAD = $(DATA_PAD)"
	@echo "JFFS = $(MKFS_JFFS2)"
	@echo "TARGET_DEVICE_TABLE=$(TARGET_DEVICE_TABLE)"
	@echo "TARGET_DIR = $(TARGET_DIR)"

jffs_info16 : 
	@echo "OPTS = $(JFFS2_OPTS_16M)"
	@echo "ROOTFS_PAD = $(JFFS2_PAD)"
	@echo "DATAFS_PAD_16M = $(DATA_PAD_16M)"
	@echo "JFFS = $(MKFS_JFFS2)"
	@echo "TARGET_DEVICE_TABLE=$(TARGET_DEVICE_TABLE)"
	@echo "TARGET_DIR = $(TARGET_DIR)"


rootfs-flash-info : redboot-flash-info kernel-flash-info 
	@echo "Flash rootfs jffs partitions using RedBoot commands:"
	@echo "  RedBoot> load -r -b %{FREEMEMLO} `basename $(JFFS2_TARGET)`"
	@echo "  RedBoot> fis create -f 0xbfc30000 -e 0  rootfs"

#
# mtd-host is a dependency which builds a local copy of mkfs.jffs2 if it's needed.
# the actual build is done from package/mtd/mtd.mk and it sets the
# value of MKFS_JFFS2 to either the previously installed copy or the one
# just built.
#
FAKEROOT_ENV=$(BUILD_DIR)/fakeroot.env
$(JFFS2_TARGET): $(TARGET_DIR) host-fakeroot makedevs $(FAKEROOT_ENV) mtd-host
	@echo "TARGET=$(JFFS2_TARGET)"
	-@find $(TARGET_DIR) -type f -perm +111 | xargs $(STRIP) 2>/dev/null || true;
	@rm -rf $(TARGET_DIR)/usr/man
	@rm -rf $(TARGET_DIR)/usr/share/man
	@rm -rf $(TARGET_DIR)/usr/info
	-/sbin/ldconfig -r $(TARGET_DIR) 2>/dev/null
	# Use fakeroot to pretend all target binaries are owned by root
	-$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(FAKEROOT_ENV) \
		-s $(FAKEROOT_ENV) -- \
		chown -R root:root $(TARGET_DIR)
	# Use fakeroot to pretend to create all needed device nodes
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(FAKEROOT_ENV) \
		-s $(FAKEROOT_ENV) -- \
		$(STAGING_DIR)/bin/makedevs \
		-d $(TARGET_DEVICE_TABLE) \
		$(TARGET_DIR)
	# Use fakeroot so mkfs.jffs2 believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(FAKEROOT_ENV) \
		-s $(FAKEROOT_ENV) -- \
		$(MKFS_JFFS2) \
			$(JFFS2_OPTS) -D $(TARGET_DEVICE_TABLE) \
			-d $(BUILD_DIR)/root \
			-o $(JFFS2_TARGET)
	@ls -l $(JFFS2_TARGET)

JFFS2_COPYTO := $(strip $(subst ",,$(BR2_TARGET_ROOTFS_JFFS2_COPYTO)))

jffs2root: jffs_info $(JFFS2_TARGET)
ifneq ($(JFFS2_COPYTO),)
	@cp -f $(JFFS2_TARGET) $(JFFS2_COPYTO)
endif

jffs2root-source: mtd-host-source

jffs2root-clean: mtd-host-clean
	-rm -f $(JFFS2_TARGET)

jffs2root-dirclean: mtd-host-dirclean
	-rm -f $(JFFS2_TARGET)


#############################################################
# flash partition addresses
#
#

PADSIZE=
ifeq ($(strip $(BR2_TARGET_FLASH_4MB)),y)
# GD6 4MB flash
# data size 0x70000
PADSIZE=0x70000
PART_ADDR=0xa8370000

else

# GD6 8MB flash
# data size 0x80000

#ifeq ($(strip $(BR2_TARGET_DRAM_16MB)),y)
## adapter16
#PART_ADDR=0xa83C0000
#else
## router32
PART_ADDR=0xa8750000
#endif

endif

ifeq ($(strip $(BR2_RKS_BOARD_RETRIEVER)),y)

# 16MB flash:  retriever ...
# data size 0x00140000
# retriever has no radio config section
# husky or gd6 boards with 16MB flash would be different

PADSIZE=0x140000
PART_ADDR=0xbfe40000

endif

ifeq ($(strip $(BR2_RKS_BOARD_GD4)),y)
# size 0x70000
PART_ADDR=0xbff40000
endif

ifeq ($(PROFILE), router32)
PART_ADDR_16M=0xa8dc0000
endif

datafs-flash-info :
	@echo
	@echo "*** If needed ***"
	@echo "Flash data jffs partition using RedBoot commands:"
ifdef OLD_CMDLINE
	@echo "  RedBoot> load -r -b %{FREEMEMLO} `basename $(DATAFS_COPYTO)`/`basename $(JFFS2_DATA)`"
else
	@echo "  RedBoot> load -r -b %{FREEMEMLO} $(TFTP_SUBDIR)`basename $(JFFS2_DATA)`"
endif
	@echo "  RedBoot> fis create -f $(PART_ADDR) -e 0  datafs"
	@echo
ifeq ($(PROFILE), router32)
	@echo "Flash data jffs partition using RedBoot commands: (16MB Flash)"
	@echo "  RedBoot> load -r -b %{FREEMEMLO} `basename $(DATAFS_COPYTO)`/`basename $(JFFS2_DATA_16M)`"
	@echo "  RedBoot> fis create -f $(PART_ADDR_16M) -e 0  datafs"
	@echo
endif

#############################################################
# 
# create empty directory for persistent data

JFFS2_DATA := $(subst ",,$(BR2_TARGET_DATAFS_JFFS2_OUTPUT))
JFFS2_DATA_16M := $(subst ",,$(BR2_TARGET_DATAFS_JFFS2_OUTPUT_16M))

TARGET_DATA := $(BUILD_DIR)/data

DATA_PADSIZE := $(strip $(subst ",,$(BR2_TARGET_DATAFS_JFFS2_PADSIZE)))
DATA_PADSIZE_16M := $(strip $(subst ",,$(BR2_TARGET_DATAFS_JFFS2_PADSIZE_16M)))

ifeq ($(DATA_PADSIZE),)
# if not specified in br2.config

ifdef PADSIZE
DATA_PAD = -p$(PADSIZE)
else
DATA_PAD = -p0x70000
endif

else

DATA_PAD = -p$(DATA_PADSIZE)

endif


ifeq ($(DATA_PADSIZE_16M),)

ifdef PADSIZE_16M
DATA_PAD_16M = -p$(PADSIZE_16M)
else
DATA_PAD_16M = -p0x180000
endif

else

DATA_PAD_16M = -p$(DATA_PADSIZE_16M)

endif

JFFS2_OPTS += $(DATA_PAD)
JFFS2_OPTS_16M += $(DATA_PAD_16M)

CHOWN_CMD =chown -R root:root $(TARGET_DATA)

DATAFS_CMDS =  $(MKFS_JFFS2) $(JFFS2_OPTS) -d $(TARGET_DATA) -o $(JFFS2_DATA)
DATAFS16_CMDS = $(MKFS_JFFS2) $(JFFS2_OPTS_16M) -d $(TARGET_DATA) -o $(JFFS2_DATA_16M)

.PHONY : _target_datafs
_target_datafs:
	@echo "TARGET_DATA_DIR=$(TARGET_DATA)"
	-mkdir -p $(TARGET_DATA)/data
	-ln -s /tmp/non_persist $(TARGET_DATA)/non_persist
	-mkdir -p $(TARGET_DATA)/etc/
	-mkdir -p $(TARGET_DATA)/certs/
	-mkdir -p $(TARGET_DATA)/certs/client-cert/
	-mkdir -p $(TARGET_DATA)/certs/ca-cert/

$(JFFS2_DATA): host-fakeroot mtd-host _target_datafs
	@echo "TARGET=$(JFFS2_DATA)"
	# Use fakeroot to pretend all target binaries are owned by root
	$(STAGING_DIR)/usr/bin/fakeroot -- /bin/sh -c "$(CHOWN_CMD) ; $(DATAFS_CMDS)"
	@ls -l $(JFFS2_DATA)

$(JFFS2_DATA_16M): host-fakeroot mtd-host _target_datafs
	@echo "TARGET=$(JFFS2_DATA_16M)"
	# Use fakeroot to pretend all target binaries are owned by root
	$(STAGING_DIR)/usr/bin/fakeroot -- /bin/sh -c "$(CHOWN_CMD) ; $(DATAFS16_CMDS)"
	@ls -l $(JFFS2_DATA_16M)

DATAFS_COPYTO := $(strip $(subst ",,$(BR2_TARGET_DATAFS_JFFS2_COPYTO)))
ifeq ($(DATAFS_COPYTO),)
DATAFS_COPYTO = $(JFFS2_COPYTO)
endif

jffs2data: jffs_info $(JFFS2_DATA)
ifneq ($(DATAFS_COPYTO),)
	-mkdir -p $(DATAFS_COPYTO)
	@cp -f $(JFFS2_DATA) $(DATAFS_COPYTO)
endif

jffs2data16: jffs_info16 $(JFFS2_DATA_16M)
ifneq ($(DATAFS_COPYTO),)
	-mkdir -p $(DATAFS_COPYTO)
	@cp -f $(JFFS2_DATA_16M) $(DATAFS_COPYTO)
endif


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_ROOTFS_JFFS2)),y)
TARGETS+=jffs2root
ROOTFS_TARGET = jffs2root
ROOTFS_OBJS = $(JFFS2_TARGET)
endif
ifeq ($(strip $(BR2_TARGET_DATAFS_JFFS2)),y)
TARGETS+=jffs2data
endif
ifeq ($(strip $(BR2_TARGET_DATAFS_JFFS2_16M)),y)
TARGETS+=jffs2data16
endif
