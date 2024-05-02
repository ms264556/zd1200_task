#############################################################
#
# Makefile for Python package 
#
#############################################################
PYTHON_VERSION=2.7.8
PYTHON_SOURCE:=Python-$(PYTHON_VERSION).tgz
PYTHON_SITE:=http://python.org/ftp/python/$(PYTHON_VERSION)
PYTHON_BUILD_DIR:=$(BUILD_DIR)/Python-$(PYTHON_VERSION)
PYTHON_CAT:=bzcat
PYTHON_BINARY:=python

#PYTHON_ROOTFS_DEPLOY_DIR:=${TARGET_DIR}/opt/Python-${PYTHON_VERSION}
PYTHON_ROOTFS_DEPLOY_DIR:=${TARGET_DIR}/usr/local
PYTHON_LIB_DEPLOY_DIR:=${PYTHON_ROOTFS_DEPLOY_DIR}/lib/python2.7
PYTHON_LIB_SITE_PACK:=${PYTHON_LIB_DEPLOY_DIR}/site-packages
PYTHON_REQ_DIR:=${PYTHON_LIB_DEPLOY_DIR}/site-packages/requests/packages/chardet
PYTHON_LIB_DYN_LOAD:=${PYTHON_LIB_DEPLOY_DIR}/lib-dynload/
PYTHON_ENC_DIR:=${PYTHON_LIB_DEPLOY_DIR}/encodings
REMOVE_PACK_1:=awslambda beanstalk cloud* codedeploy cognito configservice contrib datapipeline directconnect ec2* elastic* ecs \
		emr fps iam kinesis kms logs mturk mws opsworks rds* redshift roboto route53 sdb support swf vpc manage 
REMOVE_PACK_2=2to3 idle pydoc smtpd.py
REMOVE_PACK_3=lib2to3 bsddb pydoc_data lib-tk sqlite3 idlelib distutils
REMOVE_PACK_4=_codecs_cn.so _codecs_hk.so _codecs_jp.so _codecs_kr.so _codecs_tw.so _ctypes_test.so _testcapi.so audioop.so elementtree.so _multibytecodec.so
REMOVE_JUNK=mac* _jp* hp_roman8.py 
REMOVE_REQ_JUNK=hebrewprober.py langcyrillicmodel.py langthaimodel.py eucjpprober.py euctwprober.py langgreekmodel.py latin1prober.py utf8prober.py euckrfreq.py jpcntx.py langhebrewmodel.py  euckrprober.py  langbulgarianmodel.py  langhungarianmodel.py 
REMOVE_ENC_JUNK=*jp* *kr* mac_* *roman* *latin* tis_620.py

# Boto
BOTO_VERSION=2.36.0
BOTO_SOURCE:=boto-$(BOTO_VERSION).tar.gz
BOTO_BUILD_DIR:=${BUILD_DIR}/boto-${BOTO_VERSION}
BOTO_SITE:=https://pypi.python.org/packages/source/b/boto/${BOTO_SOURCE}

# Stormpath
SP_VERSION=1.3.2
SP_SOURCE:=stormpath-${SP_VERSION}.tar.gz
SP_BUILD_DIR:=${BUILD_DIR}/SP-${SP_VERSION}
SP_SITE:= https://pypi.python.org/packages/source/s/stormpath/${SP_SOURCE}

# Requests
RQ_VERSION=2.5.1
RQ_SOURCE:=requests-${RQ_VERSION}.tar.gz
RQ_BUILD_DIR:=${BUILD_DIR}/RQ-${RQ_VERSION}
RQ_SITE:=https://pypi.python.org/packages/source/r/requests/${RQ_SOURCE}

# Six
SIX_VERSION=1.9.0
SIX_SOURCE:=six-${SIX_VERSION}.tar.gz
SIX_BUILD_DIR:=${BUILD_DIR}/six-${SIX_VERSION}
SIX_SITE:=https://pypi.python.org/packages/source/s/six/${SIX_SOURCE}

# Flask
FLASK_VERSION=0.10.1
FLASK_SOURCE:=Flask-${FLASK_VERSION}.tar.gz
FLASK_BUILD_DIR:=${BUILD_DIR}/Flask-${FLASK_VERSION}
FLASK_SITE:=https://pypi.python.org/packages/source/F/Flask/${FLASK_SOURCE}

# Werkzeug
W_VERSION=0.10.1
W_SOURCE:=Werkzeug-${W_VERSION}.tar.gz
W_BUILD_DIR:=${BUILD_DIR}/Werkzeug-${W_VERSION}
W_SITE:=https://pypi.python.org/packages/source/W/Werkzeug/${W_SOURCE}

# Jinja2
J_VERSION=2.7.3
J_SOURCE:=Jinja2-${J_VERSION}.tar.gz
J_BUILD_DIR:=${BUILD_DIR}/Jinja2-${W_VERSION}
J_SITE:=https://pypi.python.org/packages/source/J/Jinja2/${J_SOURCE}

# MarkupSafe
M_VERSION=0.23
M_SOURCE:=MarkupSafe-${M_VERSION}.tar.gz
M_BUILD_DIR:=${BUILD_DIR}/MarkupSafe-${M_VERSION}
M_SITE:=https://pypi.python.org/packages/source/M/MarkupSafe/${M_SOURCE}

# itsdangerous
I_VERSION=0.24
I_SOURCE:=itsdangerous-${I_VERSION}.tar.gz
I_BUILD_DIR:=${BUILD_DIR}/itsdangerous-${I_VERSION}
I_SITE:=https://pypi.python.org/packages/source/i/itsdangerous/${I_SOURCE}

# pubnub
P_VERSION=3.7.1
P_SOURCE:=pubnub-${P_VERSION}.tar.gz
P_BUILD_DIR:=${BUILD_DIR}/pubnub-${P_VERSION}
P_SITE:=https://pypi.python.org/packages/source/p/pubnub/${P_SOURCE}

# pycrpto
PY_VERSION=2.6.1
PY_SOURCE:=pycrypto-${PY_VERSION}.tar.gz
PY_BUILD_DIR:=${BUILD_DIR}/pycrypto-${PY_VERSION}
PY_SITE:=https://pypi.python.org/packages/source/p/pycrypto/pycrypto-${PY_VERSION}.tar.gz

# pexpect
PE_VERSION=3.3
PE_SOURCE:=pexpect-${PE_VERSION}.tar.gz
PE_BUILD_DIR:=${BUILD_DIR}/pycrypto-${PE_VERSION}
PE_SITE:=https://pypi.python.org/packages/source/p/pexpect/pexpect-${PE_VERSION}.tar.gz

$(DL_DIR)/$(PYTHON_SOURCE):
	 $(WGET) -P $(DL_DIR) $(PYTHON_SITE)/$(PYTHON_SOURCE)

python-source: $(DL_DIR)/$(PYTHON_SOURCE)

# Unpack the tarball
$(PYTHON_BUILD_DIR)/.unpacked: $(DL_DIR)/$(PYTHON_SOURCE)
	if [ ! -d ${PYTHON_BUILD_DIR} ]; then \
		tar -xvzf $(DL_DIR)/$(PYTHON_SOURCE) -C $(BUILD_DIR); \
	fi
	touch $@

# Run the patch

$(PYTHON_BUILD_DIR)/.patched: $(PYTHON_BUILD_DIR)/.unpacked
	(cd ${PYTHON_BUILD_DIR}; \
		patch -p1 < ${TOP_DIR}/package/python/python-006-cross-compile-getaddrinfo.patch; \
		patch -p1 < ${TOP_DIR}/package/python/python-002-setup.py-ssl-zlib-path-fix.patch; \
		patch -p1 < ${TOP_DIR}/package/python/python-001-cross-compile-fix-test-grammar.patch; \
	);
	echo "ac_cv_file__dev_ptmx=no" >> $(PYTHON_BUILD_DIR)/config.site 
	echo "ac_cv_file__dev_ptc=no" >> $(PYTHON_BUILD_DIR)/config.site 
	touch $@

# Build python (host side first)
# configure & make
hostpython:$(PYTHON_BUILD_DIR)/.patched
	(cd $(PYTHON_BUILD_DIR); rm -rf config.cache; \
		./configure; \
		make python; \
		mv python hostpython; \
		mv Parser/pgen hostpgen; \
		make distclean; \
	);
	touch $@

# Cross compile now 
$(PYTHON_BUILD_DIR)/.configured:hostpython
	(cd $(PYTHON_BUILD_DIR); rm -rf config.cache; \
		OPT="$(TARGET_OPTIMIZATION)" \
		./configure \
		--enable-ipv6 \
		${TARGET_CONFIGURE_OPTS} \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_TARGET_NAME) \
		--target=$(GNU_HOST_NAME) \
		--prefix=/python \
		--sysconfdir=/etc/python2.7 \
		CONFIG_SITE=config.site; \
	);
	touch $@

# Make hostpython 
$(PYTHON_BUILD_DIR)/.build:$(PYTHON_BUILD_DIR)/.configured
	(cd $(PYTHON_BUILD_DIR); \
	$(MAKE) HOSTPYTHON=./hostpython \
		HOSTPGEN=hostpgen \
		BLDSHARED="${TARGET_CROSS}gcc -shared" \
		${TARGET_CONFIGURE_OPTS} \
	        CROSS_COMPILE_TARGET=yes \
		CROSS_COMPILE=${TARGET_CROSS} \
		HOSTARCH=${GNU_TARGET_NAME} \
		BUILDARCH=${GNU_HOST_NAME} \
	);
	touch $@

.PHONY:
$(PYTHON_BUILD_DIR)/.install:$(PYTHON_BUILD_DIR)/.build
	LD_LIBRARY_PATH=$(STAGING_DIR)/lib; \
	( cd $(PYTHON_BUILD_DIR); mkdir -p ${PYTHON_ROOTFS_DEPLOY_DIR}; \
		$(MAKE) -i install HOSTPYTHON=./hostpython \
			BLDSHARED="${TARGET_CROSS}gcc -shared" \
			CC=${TARGET_CROSS}gcc \
			CXX=${TARGET_CROSS}g++ \
			AR=${TARGET_CROSS}ar \
			RANLIB=${TARGET_CROSS}ranlib \
	       		CROSS_COMPILE_TARGET=yes \
			CROSS_COMPILE=${TARGET_CROSS} \
			LDFLAGS="-L${TARGET_DIR}/lib -L${TARGET_DIR}/usr/lib -L${TARGET_DIR}/usr/local/lib" \
			prefix=${PYTHON_ROOTFS_DEPLOY_DIR} \
	)
	(cd ${TARGET_DIR}; \
		ln -fs /usr/local/bin/python2.7 usr/bin/python; \
	)

.PHONY:
misc:$(PYTHON_BUILD_DIR)/.install
	tar -zxvf $(DL_DIR)/$(BOTO_SOURCE) -C $(BUILD_DIR); \
        (cd ${BUILD_DIR}/boto-${BOTO_VERSION}; \
		python setup.py build; \
		python setup.py install --root=../root/; \
	)
	tar -zxvf $(DL_DIR)/$(RQ_SOURCE) -C $(BUILD_DIR); \
	(cd ${BUILD_DIR}/requests-${RQ_VERSION}; \
		python setup.py build; \
		python setup.py install --root=../root/; \
	)
	touch $@
	tar -zxvf $(DL_DIR)/$(P_SOURCE) -C $(BUILD_DIR); \
	(cd ${BUILD_DIR}/pubnub-${P_VERSION}; \
		python setup.py build; \
		python setup.py install --root=../root/; \
	)
	tar -zxvf $(DL_DIR)/$(PE_SOURCE) -C $(BUILD_DIR); \
	(cd ${BUILD_DIR}/pexpect-${PE_VERSION}; \
		python setup.py build; \
		python setup.py install --root=../root/; \
	)
	tar -zxvf $(DL_DIR)/$(PY_SOURCE) -C $(BUILD_DIR); \
	(export LD_LIBRARY_PATH=$(STAGING_DIR)/lib; \
		export CC="${TARGET_CROSS}gcc" \
		export CCSHARED="${TARGET_CROSS}gcc -shared" \
		export CXX="${TARGET_CROSS}g++" \
		export AR="${TARGET_CROSS}ar" \
		export RANLIB=${TARGET_CROSS}ranlib \
		export CROSS_COMPILE_TARGET=yes \
		export CROSS_COMPILE=${TARGET_CROSS} \
		export CROSS_LD="${TARGET_CROSS}ld" \
		export CFLAGS="-I${TARGET_DIR}/usr/local/include/python2.7/ -I${PYTHON_BUILD_DIR}/Include/" \
		export LDSHARED="${TARGET_CROSS}gcc -pthread -shared"; \
		cd ${BUILD_DIR}/pycrypto-${PY_VERSION}; \
		export LD_LIBRARY_PATH=$(STAGING_DIR)/lib; \
		./configure \
			--build=$(GNU_HOST_NAME) \
			--host=$(GNU_TARGET_NAME) \
			--target=$(GNU_HOST_NAME) \
			CC="${TARGET_CROSS}gcc" \
			CROSS_COMPILE_TARGET=yes \
			LDFLAGS="-L${TARGET_DIR}/lib \
			-L${TARGET_DIR}/usr/lib  \
			-L${TARGET_DIR}/usr/local/lib"; \
			python setup.py build; \
			python setup.py install --root=../root/; \
	)

markup:
	export LD_LIBRARY_PATH=$(STAGING_DIR)/lib; \
	export CC="${TARGET_CROSS}gcc" \
	export CCSHARED="${TARGET_CROSS}gcc -shared" \
	export CXX="${TARGET_CROSS}g++" \
	export AR="${TARGET_CROSS}ar" \
	export RANLIB=${TARGET_CROSS}ranlib \
	export CROSS_COMPILE_TARGET=yes \
	export CROSS_COMPILE=${TARGET_CROSS} \
	export CFLAGS="-I${TARGET_DIR}/usr/local/include/python2.7/ -I${PYTHON_BUILD_DIR}/Include/"; \
	tar -zxvf $(DL_DIR)/$(M_SOURCE) -C $(BUILD_DIR); \
	(cd ${BUILD_DIR}/MarkupSafe-${M_VERSION}; \
		CC=${TARGET_CROSS}gcc \
		CXX=${TARGET_CROSS}g++ \
		AR=${TARGET_CROSS}ar \
		RANLIB=${TARGET_CROSS}ranlib \
	       	CROSS_COMPILE_TARGET=yes \
		CROSS_COMPILE=${TARGET_CROSS} \
		LDFLAGS="-L${TARGET_DIR}/lib \
			-L${TARGET_DIR}/usr/lib  \
			-L${TARGET_DIR}/usr/local/lib" \
		python setup.py build; \
		${TARGET_CROSS}gcc -shared ${CFLAGS} ${M_BUILD_DIR}/build/temp.linux-x86_64-2.7/markupsafe/_speedups.o -o ${M_BUILD_DIR}/build/lib.linux-x86_64-2.7/markupsafe/_speedups.so; \
		python setup.py install --root=../root/; \
	)

.PHONY:
trim-packages:
	(cd ${PYTHON_LIB_SITE_PACK}; \
		/bin/rm -rf Crypto/Random Crypto/SelfTest; \
	)
	(cd ${PYTHON_LIB_SITE_PACK}/boto; \
		/bin/rm -rf ${REMOVE_PACK_1}; \
	)
	(cd ${PYTHON_ROOTFS_DEPLOY_DIR}/bin/; \
		/bin/rm -rf ${REMOVE_PACK_2}; \
	)
	(cd ${PYTHON_LIB_DEPLOY_DIR}; \
		/bin/rm -rf ${REMOVE_PACK_3}; \
		/bin/rm -rf config/libpython2.7.a; \
	)
	(cd ${PYTHON_LIB_DYN_LOAD}; \
		/bin/rm -rf ${REMOVE_PACK_4}; \
	)
	(cd ${PYTHON_LIB_DEPLOY_DIR}; \
		/bin/rm -rf ${REMOVE_JUNK}; \
	)
	(cd ${PYTHON_REQ_DIR}; \
		/bin/rm -rf ${REMOVE_REQ_JUNK}; \
	)
	(cd ${PYTHON_ENC_DIR}; \
		/bin/rm -rf ${REMOVE_ENC_JUNK}; \
	)
	/bin/rm -rf ${PYTHON_ROOTFS_DEPLOY_DIR}/share
	find ${PYTHON_ROOTFS_DEPLOY_DIR} -name *.pyc | /bin/rm -rf `xargs $1`
	${TARGET_CROSS}strip -s ${PYTHON_ROOTFS_DEPLOY_DIR}/bin/python2.7
	/bin/rm -rf ${PYTHON_ROOTFS_DEPLOY_DIR}/lib/libpython2.7.a

.PHONY:
trim_tests:misc markup trim-packages
	$(warning ### Nuke all the test folders ###)
	find ${TARGET_DIR}/usr/local/lib/ -name test* -print | /bin/rm -rf `xargs $1`
	find ${TARGET_DIR}/usr/local/lib/ -name unittest -print | /bin/rm -rf `xargs $1`
	find ${TARGET_DIR}/usr/local/lib/ -name *.so -print | ${TARGET_CROSS}strip --strip-unneeded `xargs $1`
	find ${TARGET_DIR}/usr/local/lib/ -name *.o -print | ${TARGET_CROSS}strip --strip-unneeded `xargs $1`

#Target (without regression tests)
cross-python:trim_tests

cp:cross-python-dirclean cross-python

#Target to build (to include regression test suites as well)
cross-python-test:$(PYTHON_BUILD_DIR)/.trim

cross-python-clean:
	-$(MAKE) -C ${PYTHON_BUILD_DIR} distclean

cross-python-dirclean:
	/bin/rm -rf ${PYTHON_BUILD_DIR}
	/bin/rm -rf ${PYTHON_ROOTFS_DEPLOY_DIR}
	/bin/rm -rf $(BUILD_DIR)/$(BOTO_SOURCE)
	/bin/rm -rf $(BUILD_DIR)/$(SP_SOURCE)

#############################################################
#
# Toplevel Makefile options
#
#############################################################
ifeq ($(strip $(BR2_PACKAGE_PYTHON)),y)
TARGETS+=cross-python
endif
