#############################################################
#
# memcached
#
#############################################################

MEMCACHED_SRC_DIR := ${V54_DIR}/apps/memcached

MEMCACHED_BUILD_DIR := ${BUILD_DIR}/memcached
MEMCACHED_BUILD_BIN := $(MEMCACHED_BUILD_DIR)/memcached
MEMCACHED_BUILD_LIB := $(MEMCACHED_BUILD_DIR)/libmemcached.so

MEMCACHED_TARGET_BIN := $(TARGET_DIR)/usr/sbin/memcached
MEMCACHED_TARGET_LIB := $(TARGET_DIR)/usr/lib/libmemcached.so

MEMCACHED_MAKE_OPT := -C $(MEMCACHED_BUILD_DIR) -f $(MEMCACHED_SRC_DIR)/Makefile.br

MEMCACHED_CFLAGS = $(TARGET_CFLAGS) -MMD -Werror

ifeq ($(BR2_GCC_VERSION_4_3_X),y)
MEMCACHED_CFLAGS += -fPIC
else
ifeq ($(BR2_ARCH),"powerpc")
MEMCACHED_CFLAGS += -fPIC
endif
endif

memcached: uclibc
	@mkdir -p $(MEMCACHED_BUILD_DIR)
	$(MAKE) $(MEMCACHED_MAKE_OPT) CC=$(TARGET_CC) MEMCACHED_SRC_DIR=$(MEMCACHED_SRC_DIR) \
		MEMCACHED_CFLAGS="$(MEMCACHED_CFLAGS)" MEMCACHED_LDFLAGS="$(MEMCACHED_LDFLAGS)"
	install -D $(MEMCACHED_BUILD_BIN) $(MEMCACHED_TARGET_BIN);
	install -D $(MEMCACHED_BUILD_LIB) $(MEMCACHED_TARGET_LIB);

memcached-clean:
	rm -f $(MEMCACHED_TARGET_BIN) $(MEMCACHED_TARGET_LIB)
	-$(MAKE) $(MEMCACHED_MAKE_OPT) clean

memcached-dirclean:
	rm -rf $(MEMCACHED_BUILD_DIR)


#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_MEMCACHED)),y)
TARGETS+=memcached
endif
