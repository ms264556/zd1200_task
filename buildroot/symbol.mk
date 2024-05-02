# this file implements a mechanism for easily adding files into symbol-pack.tgz

# $(RELEASE_COPYTO) is defined in release.mk
RKS_SYMBOL_PACK_TGZ = $(RELEASE_COPYTO)/symbol-pack.tgz
RKS_SYMBOL_PACK_TAR = $(RKS_SYMBOL_PACK_TGZ:.tgz=.tar)

# rks-symbol-pack-init needs to be built before any other symbol pack targets
POST_TARGETS += rks-symbol-pack-init

# $(call RKS_SYMBOL_PACK,<target>,<src-file>,<dst-directory>)
# generate a target <target> to add <src-file> in the result symbol-pack.tgz in direcotry <dst-directory>
define RKS_SYMBOL_PACK
$(if $(filter %/,$(3)),,$(error argument 3 of RKS_SYMBOL_PACK "$(3)" should ends with "/"))$(eval\
$(1):$(sep)
	$(TAR) rvf $$(RKS_SYMBOL_PACK_TAR) -P --transform 's!.*/\(.*\)!$(strip $(3))\1!' $(2)$(sep)\
POST_TARGETS += $(1)$(sep)
)
endef

.PHONY: rks-symbol-pack-init
rks-symbol-pack-init:
	rm -f $(RKS_SYMBOL_PACK_TAR)

.PHONY: rks-symbol-pack-release
rks-symbol-pack-release:
	gzip < $(RKS_SYMBOL_PACK_TAR) > $(RKS_SYMBOL_PACK_TGZ);rm -f $(RKS_SYMBOL_PACK_TAR)

# make rks-symbol-pack-release being built before flash-info
flash-info: rks-symbol-pack-release


