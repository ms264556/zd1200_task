#############################################################
#
# iperf
#
#############################################################

IPERF_VER:=2.0.5
IPERF_SRC_DIR:=$(TOP_DIR)/../video54/apps/iperf-$(IPERF_VER)
IPERF_DIR:=$(BUILD_DIR)/iperf-$(IPERF_VER)

$(IPERF_DIR)/configure:
	@mkdir -p $(IPERF_DIR)
	cp -R $(IPERF_SRC_DIR)/* $(IPERF_DIR)
	touch $(IPERF_DIR)/configure

$(IPERF_DIR)/config.h: $(IPERF_DIR)/configure
	( \
		cd $(IPERF_DIR) ; \
		rm -f $(IPERF_DIR)/config.h ; \
		./configure \
	)

$(IPERF_DIR)/src/iperf: $(IPERF_DIR)/config.h
	$(MAKE) \
		-C $(IPERF_DIR) CC=$(TARGET_CC) CXX=$(TARGET_CPP)

$(TARGET_DIR)/sbin/iperf: $(IPERF_DIR)/src/iperf
	cp -af $< $@

iperf: uclibc libRutil $(TARGET_DIR)/sbin/iperf

iperf-clean:
	rm -f $(TARGET_DIR)/sbin/iperf
	-$(MAKE) -C $(IPERF_DIR) clean

iperf-dirclean:
	rm -rf $(IPERF_DIR)
#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_IPERF)),y)
TARGETS+=iperf
# need LIBSTDCPP
# override the br2.config setting
BR2_JUNK_LIBSTDCPP=
endif
