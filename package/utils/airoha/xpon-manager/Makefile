# SPDX-License-Identifier: GPL-2.0-only

include $(TOPDIR)/rules.mk

PKG_NAME:=xpon-manager
PKG_VERSION:=1.2.0
PKG_RELEASE:=1
PKG_MAINTAINER:=Matheus Sampaio Queiroga <srherobrine20@gmail.com>
PKG_LICENSE:=GPL-2.0-only

include $(INCLUDE_DIR)/package.mk

define Package/xpon-manager
  SECTION:=utils
  CATEGORY:=Utilities
  SUBMENU:=Airoha
  TITLE:=Native userspace control plane for Linux xPON, OAM and OMCI
  DEPENDS:=+ucode +libucode +ucode-mod-ubus +ucode-mod-uci +ucode-mod-uloop +rpcd +rpcd-mod-ucode +libuci +libubus
endef

define Package/xpon-manager/description
 Native C ucode bindings for the Linux net/xpon and net/xpon/omci Generic
 Netlink APIs, rpcd services exposing xPON/OAM and OMCI over ubus, and
 xponctl/omcictl clients. The package does not use sysfs for kernel control.
endef

define Build/Prepare
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/. $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-fPIC -Wall -Wextra -Werror \
		-I$(STAGING_DIR)/usr/include \
		-o $(PKG_BUILD_DIR)/omci.so \
		$(PKG_BUILD_DIR)/omci_ucode.c \
		-shared $(TARGET_LDFLAGS) -lucode
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-fPIC -Wall -Wextra -Werror \
		-I$(STAGING_DIR)/usr/include \
		-o $(PKG_BUILD_DIR)/xpon.so \
		$(PKG_BUILD_DIR)/xpon_ucode.c \
		-shared $(TARGET_LDFLAGS) -lucode
endef

define Package/xpon-manager/install
	$(INSTALL_DIR) $(1)/usr/lib/ucode
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/omci.so $(1)/usr/lib/ucode/omci.so
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/xpon.so $(1)/usr/lib/ucode/xpon.so

	$(INSTALL_DIR) $(1)/usr/share/rpcd/ucode
	$(INSTALL_BIN) ./files/usr/share/rpcd/ucode/xpon $(1)/usr/share/rpcd/ucode/xpon

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/usr/sbin/omcictl $(1)/usr/sbin/omcictl
	$(INSTALL_BIN) ./files/usr/sbin/xponctl $(1)/usr/sbin/xponctl

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/xpon $(1)/etc/config/xpon
endef

define Package/xpon-manager/conffiles
/etc/config/xpon
endef

define Package/xpon-manager/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || /etc/init.d/rpcd reload >/dev/null 2>&1 || true
endef

$(eval $(call BuildPackage,xpon-manager))
