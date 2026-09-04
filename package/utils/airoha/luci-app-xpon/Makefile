# SPDX-License-Identifier: GPL-2.0-only

include $(TOPDIR)/rules.mk

PKG_VERSION:=1.2.0
PKG_RELEASE:=1
LUCI_TITLE:=LuCI support for the Linux xPON control plane
LUCI_PKGARCH:=all
PKG_MAINTAINER:=Matheus Sampaio Queiroga <srherobrine20@gmail.com>
PKG_LICENSE:=GPL-2.0-only
LUCI_DEPENDS:=+luci-base +xpon-manager @TARGET_airoha

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
