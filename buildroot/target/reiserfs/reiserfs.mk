#############################################################
#
# Build the reiser root filesystem image
#
#############################################################
REISERFS_OPTS = -f -f --format 3.6
REISERFS_BLOCK_SIZE = 4096
REISERFS_JOURNAL_BLOCKS = 8193

ifneq ($(strip $(BR2_TARGET_DATAFS_BLOCK_SIZE)),"0")
REISERFS_BLOCK_SIZE = $(subst ",,$(BR2_TARGET_DATAFS_BLOCK_SIZE))
endif
ifneq ($(strip $(BR2_TARGET_DATAFS_JOURNAL_BLOCKS)),"0")
REISERFS_JOURNAL_BLOCKS = $(subst ",,$(BR2_TARGET_DATAFS_JOURNAL_BLOCKS))
endif

REISERFS_OPTS += -s $(REISERFS_JOURNAL_BLOCKS) -b $(REISERFS_BLOCK_SIZE)

#############################################################
# 
# create empty directory for persistent data

REISERFS_DATA := $(subst ",,$(BR2_TARGET_DATAFS_REISERFS_OUTPUT))
TARGET_DATA := $(BUILD_DIR)/data
TEMP_DATA_DIR := $(BUILD_DIR)/reiserfs_tmp

$(REISERFS_DATA): host-fakeroot $(STAGING_DIR)/fakeroot.env mtd-host
	@echo "TARGET=$(REISERFS_DATA)"
	@echo "TARGET_DATA_DIR=$(TARGET_DATA)"
	-mkdir -p $(TEMP_DATA_DIR)
	-mkdir -p $(TARGET_DATA)/data
	-ln -s /tmp/non_persist $(TARGET_DATA)/non_persist
	-mkdir -p $(TARGET_DATA)/etc/
	-mkdir -p $(TARGET_DATA)/certs/
	-mkdir -p $(TARGET_DATA)/certs/client-cert/
	-mkdir -p $(TARGET_DATA)/certs/ca-cert/
	-touch $(REISERFS_DATA)

	# Use fakeroot so genext2fs believes the previous fakery
ifeq ($(strip BR2_TARGET_DATAFS_BLOCKS)),0)
	GENREISERFS_REALSIZE=`LANG=C du -l -s -c -k $(TARGET_DATA) | grep total | sed -e "s/total//"`; \
	GENREISERFS_SIZE=`expr $$GENREISERFS_REALSIZE + $(REISERFS_JOURNAL_BLOCKS)`; \
	GENREISERFS_SIZE=`expr $$GENREISERFS_SIZE + $(subst ",,$(BR2_TARGET_DATAFS_RESERVED_BLOCKS))`; \
	set -x; \
	dd if=/dev/zero of=$(REISERFS_DATA) bs=$(REISERFS_BLOCK_SIZE) count=$$GENREISERFS_SIZE; 
else
	GENREISERFS_REALSIZE=$(BR2_TARGET_DATAFS_BLOCKS); \
	GENREISERFS_SIZE=`expr $$GENREISERFS_REALSIZE + $(REISERFS_JOURNAL_BLOCKS)`; \
	GENREISERFS_SIZE=`expr $$GENREISERFS_SIZE + $(subst ",,$(BR2_TARGET_DATAFS_RESERVED_BLOCKS))`; \
	set -x; \
	dd if=/dev/zero of=$(REISERFS_DATA) bs=$(REISERFS_BLOCK_SIZE) count=$$GENREISERFS_SIZE;
endif
	# Use fakeroot so mkreiserfs believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		$(MKFS_REISERFS) \
			$(REISERFS_OPTS)  \
			$(REISERFS_DATA)
	@ls -l $(REISERFS_DATA)

	# Use fakeroot so mkreiserfs believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		sudo mount \
			-o loop -t reiserfs \
			$(REISERFS_DATA) \
			$(TEMP_DATA_DIR)

	# Use fakeroot so mkreiserfs believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		cp -a $(TARGET_DATA)/* $(TEMP_DATA_DIR)

ifeq ($(strip $(BR2_TARGET_DATAFS_REISERFS_ROOT)),y)
	# Use fakeroot to pretend all target binaries are owned by root
	-$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		chown -R root:root $(TEMP_DATA_DIR)
endif
	
	# Use fakeroot so mkreiserfs believes the previous fakery
	$(STAGING_DIR)/usr/bin/fakeroot \
		-i $(STAGING_DIR)/fakeroot.env \
		-s $(STAGING_DIR)/fakeroot.env -- \
		sudo umount \
			$(TEMP_DATA_DIR)

REISERFSDATAFS_COPYTO := $(strip $(subst ",,$(BR2_TARGET_DATAFS_REISERFS_COPYTO)))
ifeq ($(REISERFSDATAFS_COPYTO),)
REISERFSDATAFS_COPYTO = $(REISERFS_COPYTO)
endif

reiserfsdata: reiserfsprogs_host $(REISERFS_DATA)
ifneq ($(REISERFSDATAFS_COPYTO),)
	-mkdir -p $(REISERFSDATAFS_COPYTO)
	@cp -f $(REISERFS_DATA) $(REISERFSDATAFS_COPYTO)
endif

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_TARGET_DATAFS_REISERFS)),y)
TARGETS+=reiserfsdata
endif
