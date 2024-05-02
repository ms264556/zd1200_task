DMALLOC_DIR := $(BUILD_DIR)/dmalloc
DMALLOC_SRC := $(V54_DIR)/apps/dmalloc-5.5.2
DMALLOC_CFLAGS := -DDMALLOC -I$(DMALLOC_DIR) 
DMALLOC_LIB := -L$(DMALLOC_DIR) -ldmallocth -ldmallocthcxx

dmalloc: dmalloc-install

dmalloc-build: $(DMALLOC_DIR)/Makefile
	$(MAKE) -C $(DMALLOC_DIR) 

dmalloc-install: dmalloc-build
	$(INSTALL) -m 755 $(DMALLOC_DIR)/dmalloc $(TARGET_DIR)/bin

dmalloc-clean:
	$(MAKE) -C $(DMALLOC_DIR) clean

dmalloc-dirclean:
	rm -rf $(DMALLOC_DIR)

$(DMALLOC_DIR)/Makefile:
	mkdir -p $(DMALLOC_DIR)
	cd $(DMALLOC_DIR) && CC=$(TOOLPATH)/bin/$(ARCH)-linux-gcc CXX=$(TOOLPATH)/bin/$(ARCH)-linux-g++ $(DMALLOC_SRC)/configure --prefix="/usr" --host=$(ARCH)-linux --enable-threads

ifeq ($(strip $(BR2_PACKAGE_DMALLOC)),y)
TARGETS += dmalloc
endif