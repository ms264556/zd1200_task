############################################################
#
# reiserfsprogs for host 
#
############################################################
REISERFSPROGS_VER:=3.6.6
REISERPROGS_HOST_DIR:= $(TOOL_BUILD_DIR)/reiserfsprogs-$(REISERFSPROGS_VER)
MKFS_REISERFS=$(REISERPROGS_HOST_DIR)/mkreiserfs/mkreiserfs

$(REISERPROGS_HOST_DIR)/.unpacked: $(DL_DIR)/$(REISERFSPROGS_SOURCE)
	$(REISERFSPROGS_CAT) $(DL_DIR)/$(REISERFSPROGS_SOURCE) | tar -C $(TOOL_BUILD_DIR) $(TAR_OPTIONS) -
	toolchain/patch-kernel.sh $(REISERPROGS_HOST_DIR) package/reiserfsprogs/ reiserfsprogs\*.patch
	touch $(REISERPROGS_HOST_DIR)/.unpacked

$(REISERPROGS_HOST_DIR)/.configured: $(REISERPROGS_HOST_DIR)/.unpacked
	(cd $(REISERPROGS_HOST_DIR); rm -rf config.cache; \
		./configure \
	);
	touch  $(REISERPROGS_HOST_DIR)/.configured

$(REISERPROGS_HOST_DIR)/mkreiserfs/mkreiserfs: $(REISERPROGS_HOST_DIR)/.configured
	$(MAKE1) PATH=$(TARGET_PATH) DESTDIR=$$d -C $(REISERPROGS_HOST_DIR)
	touch -c $(MKFS_REISERFS)

reiserfsprogs_host: $(MKFS_REISERFS)
	@echo "reiserfsprogs for host is ready."

reiserfsprogs_host-clean:
	-$(MAKE1) -C $(REISERPROGS_HOST_DIR) clean

reiserfsprogs_host-dirclean:
	rm -rf $(REISERPROGS_HOST_DIR)
