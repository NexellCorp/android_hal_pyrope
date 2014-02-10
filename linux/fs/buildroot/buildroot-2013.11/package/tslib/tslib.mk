################################################################################
#
# tslib/tslib-mt
#
################################################################################

ifeq ($(BR2_PACKAGE_TSLIB_LEGACY), y)

TSLIB_VERSION = 1.1
TSLIB_SITE = http://github.com/kergoth/tslib/tarball/$(TSLIB_VERSION)
TSLIB_LICENSE = GPL, LGPL
TSLIB_LICENSE_FILES = COPYING

TSLIB_AUTORECONF = YES
TSLIB_INSTALL_STAGING = YES
TSLIB_INSTALL_STAGING_OPT = DESTDIR=$(STAGING_DIR) LDFLAGS=-L$(STAGING_DIR)/usr/lib install

$(eval $(autotools-package))

else

TSLIB_VERSION = 1.0_mt
TSLIB_SOURCE = tslib-$(TSLIB_VERSION).rar
TSLIB_DL_PATH = $(BUILD_DIR)/tslib-$(TSLIB_VERSION)
TSLIB_CONF_FILE = $(TSLIB_DL_PATH)/etc/ts.conf
TSLIB_CONFIGURE_AC = $(TSLIB_DL_PATH)/configure.ac
TSLIB_SITE = http://downloads.sourceforge.net/project/tslib-mt/src
TSLIB_LICENSE = GPL, LGPL
TSLIB_LICENSE_FILES = COPYING

TSLIB_AUTORECONF = YES
TSLIB_INSTALL_STAGING = YES
TSLIB_INSTALL_STAGING_OPT = DESTDIR=$(STAGING_DIR) LDFLAGS=-L$(STAGING_DIR)/usr/lib install

define TSLIB_EXTRACT_CMDS
	unrar x $(DL_DIR)/$(TSLIB_SOURCE) $(BUILD_DIR);
endef

TSLIB_CONF_ENV = ac_cv_func_malloc_0_nonnull=yes

define TSLIB_AUTOGEN
    (cd $(@D); \
    $(shell sed -i -e 's/LT_RELEASE=0.0/LT_RELEASE=1.0/g' $(TSLIB_CONFIGURE_AC))	\
    chmod 777 autogen.sh; ./autogen.sh;	\
    $(shell sed -i -e 's/module_raw input/module_raw input\nmodule_raw mtinput/g' $(TSLIB_CONF_FILE))	\
    )
endef

TSLIB_PRE_CONFIGURE_HOOKS += TSLIB_AUTOGEN

$(eval $(autotools-package))
endif
