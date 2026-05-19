#!/bin/sh
# OpenWrt installer for Ubiquiti UniFi Travel Router (UTR)
# Run directly on the UTR:
#   curl -s https://raw.githubusercontent.com/divinehawk/openwrt/main/install-utr.sh | sh 

set -e

FIRMWARE_URL="https://github.com/divinehawk/openwrt/releases/download/build-17/openwrt-ipq40xx-generic-ubnt_utr-squashfs-factory.ubi"
FIRMWARE_FILE="/tmp/factory.ubi"

# Check boot slot
echo "==> Checking boot slot..."
BOOTID=$(grep -o 'ubntbootid=[0-9]' /proc/cmdline | cut -d= -f2)
if [ "$BOOTID" = "1" ]; then
    echo "ERROR: UTR is booted from kernel1 slot (ubntbootid=1)."
    echo "       Please upgrade/downgrade stock firmware to switch back to kernel0, then retry."
    exit 1
fi
echo "    OK: booted from kernel0 slot."

# Download firmware
echo "==> Downloading firmware..."
curl -L -o "$FIRMWARE_FILE" "$FIRMWARE_URL"

# Switch boot slot to kernel1
echo "==> Switching boot slot to kernel1..."
printf '\xff\xff\xff\xff\x2b\xe8\x4d\xa3' > /tmp/bs.bin
mtd -e bs write /tmp/bs.bin bs
rm -f /tmp/bs.bin

# Set up uboot environment
echo "==> Configuring U-Boot environment..."
fw_setenv -s - <<-'EOF'
	bootopenwrt fdt addr ${fdtcontroladdr}; fdt rm /signature; bootubnt
	bootcmd_real sf probe; sf read 0x80000000 0x2f0000 0x800; mw.b 0x80010000 0xff 0x1; if cmp.b 0x80000000 0x80010000 0x1; then echo "Slot 1 / kernel1"; run bootopenwrt; else echo "Slot 0 / kernel0"; bootubnt; fi
EOF

# Flash firmware
echo "==> Flashing OpenWrt firmware..."
MTD="$(grep -w '"kernel1"' /proc/mtd | cut -d: -f1)"
ubidetach -p /dev/$MTD 2>/dev/null || true
ubiformat /dev/$MTD -y -f "$FIRMWARE_FILE"

echo ""
echo "==> Done! Rebooting into OpenWrt..."
echo "    Connect to 192.168.1.1 after it comes up."
reboot

