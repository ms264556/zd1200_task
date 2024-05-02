#############################################################
#
# mosquitto_lbs_sub
#
#############################################################
SOVERSION                        := 1
MOSQUITTO_VER                    := 1.2
MOSQUITTO_SRC_DIR                := $(V54_DIR)/apps/mosquitto-$(MOSQUITTO_VER)
MOSQUITTO_BUILD_DIR              := $(BUILD_DIR)/mosquitto-$(MOSQUITTO_VER)
MOSQUITTO_LD                     := $(MOSQUITTO_BUILD_DIR)/lib/libmosquitto.so.$(SOVERSION)
MOSQUITTO_LBS_SUB_BUILD_DIR      := $(BUILD_DIR)/mosquitto_lbs_sub
MOSQUITTO_LBS_SUB_SRC_DIR        := $(V54_DIR)/apps/mosquitto_lbs_sub
MOSQUITTO_LBS_SUB_BINARY         := mosquitto_lbs_sub
MOSQUITTO_LBS_SUB_TARGET_BINARY  := $(TARGET_DIR)/bin/$(MOSQUITTO_LBS_SUB_BINARY)
MOSQUITTO_LBS_SUB_MAKE_OPT       := -C $(MOSQUITTO_LBS_SUB_BUILD_DIR) -f $(MOSQUITTO_LBS_SUB_SRC_DIR)/Makefile

MOSQUITTO_LBS_SUB_CFLAGS = $(TARGET_CFLAGS)
MOSQUITTO_LBS_SUB_CFLAGS += -I $(MOSQUITTO_SRC_DIR)/lib \
                -I$(V54_DIR)/../controller/ac/include \
                -I$(V54_DIR)/../controller/ac/usr/include \
                -I$(V54_DIR)/../controller/ac/usr/libsrc/utils \
                -I$(V54_DIR)/../controller/ac/usr/libsrc/emf \
                -I$(V54_DIR)/../controller/common/af_lwapp \
                -I$(V54_DIR)/../controller/common/cfg \
                -I$(V54_DIR)/../controller/common/include \
                -I$(V54_DIR)/../controller/common/lwapp/include \
                -I$(V54_DIR)/lib/librkscrypto -I$(V54_DIR)/lib/librkshash \
                -I$(V54_DIR)/lib/libcache_pool -I$(V54_DIR)/lib/libipc_sock

MOSQUITTO_LBS_SUB_LDFLAGS = $(TARGET_LDFLAGS)
MOSQUITTO_LBS_SUB_LDFLAGS +=-L$(TARGET_DIR)/usr/lib/ -lrkscrypto -lrkshash -lipc_sock -lcache_pool -lc2lib
MOSQUITTO_LBS_SUB_LDFLAGS +=-L$(TARGET_DIR)/lib/ -lemf -lm -lpthread
MOSQUITTO_LBS_SUB_LDFLAGS +=-L$(TARGET_DIR)/../controller/ac/usr/lib/ -ltu_log
MOSQUITTO_LBS_SUB_LDFLAGS +=$(MOSQUITTO_LD)

mosquitto_lbs_sub:
	@mkdir -p $(MOSQUITTO_LBS_SUB_BUILD_DIR)
	$(MAKE) $(MOSQUITTO_LBS_SUB_MAKE_OPT) CC=$(TARGET_CC) MOSQUITTO_LBS_SUB_SRC_DIR="$(MOSQUITTO_LBS_SUB_SRC_DIR)" MOSQUITTO_LBS_SUB_CFLAGS="$(MOSQUITTO_LBS_SUB_CFLAGS)" MOSQUITTO_LBS_SUB_LDFLAGS="$(MOSQUITTO_LBS_SUB_LDFLAGS)"
	install -D $(MOSQUITTO_LBS_SUB_BUILD_DIR)/$(MOSQUITTO_LBS_SUB_BINARY) $(MOSQUITTO_LBS_SUB_TARGET_BINARY)

mosquitto_lbs_sub-clean:
	rm -f $(MOSQUITTO_LBS_SUB_BINARY)
	-$(MAKE) $(MOSQUITTO_LBS_SUB_OPT) clean
	-$(MAKE) -C $(MOSQUITTO_LBS_SUB_SRC_DIR) clean

mosquitto_lbs_sub-dirclean: mosquitto_lbs_sub-clean
	rm -rf $(MOSQUITTO_LBS_SUB_BUILD_DIR)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_MOSQUITTO_LBS_SUB)),y)
TARGETS += mosquitto_lbs_sub
endif
