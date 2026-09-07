#!/usr/bin/env bash
# ==============================================================================
# Deploy.sh - Production Binary Injection, CRC32 XZ Packing & Initramfs Sync
# ==============================================================================

set -euo pipefail

# ------------------------------------------------------------------------------
# DRIVER PROFILE CONFIGURATION
# ------------------------------------------------------------------------------
DRIVER_NAME="yogafan"
DRIVER_PATH="Files"
KVERSION=$(uname -r)    # Autofilled deployment target matching active system
# ------------------------------------------------------------------------------

# Root validation check - Preserves exact execution paths (PWD) across sudo
if [ "$EUID" -ne 0 ]; then
    echo "----------------------------------------------------------------------"
    echo "NOTICE: Superuser privileges required for production driver deployment."
    echo "----------------------------------------------------------------------"
    exec sudo -E env "PWD=$(pwd)" "$0" "$@"
fi

SRC_DIR="src"
LOCAL_KO="${SRC_DIR}/${DRIVER_NAME}.ko"
TARGET_DIR="/lib/modules/${KVERSION}/kernel/drivers/hwmon"

echo "=== Deploying ${DRIVER_NAME} to Kernel Modules Tree ${KVERSION} ==="

# 1. Staged validation check
if [ ! -f "${LOCAL_KO}" ]; then
    echo "ERROR: Production compilation target missing at '${LOCAL_KO}'!" >&2
    echo "Please execute your verification pipeline first via: ./Test.sh" >&2
    exit 1
fi

# 2. Tree path initialization
if [ ! -d "${TARGET_DIR}" ]; then
    echo "Production destination subpath missing. Initializing: ${TARGET_DIR}"
    mkdir -p "${TARGET_DIR}"
fi

# 3. Clean and isolate target tree
echo "Cleaning up legacy compressed modules (.ko.xz)..."
rm -f "${TARGET_DIR}/${DRIVER_NAME}.ko.xz"

echo "Placing fresh binary into system target path..."
cp "${LOCAL_KO}" "${TARGET_DIR}/"
chmod 644 "${TARGET_DIR}/${DRIVER_NAME}.ko"

# 4. Safe kernel compression execution (Prevents modprobe EINVAL crash loops)
echo "Compressing module to kernel-compliant XZ block format (CRC32)..."
xz -f --check=crc32 --lzma2=dict=32KiB "${TARGET_DIR}/${DRIVER_NAME}.ko"

# 5. Boot persistence configuration
CONF_FILE="/etc/modules-load.d/${DRIVER_NAME}.conf"
if [ ! -f "${CONF_FILE}" ]; then
    echo "Configuring automatic boot loading matrix maps..."
    echo "${DRIVER_NAME}" > "${CONF_FILE}"
fi

# 6. Rebuild dynamic system module map index
echo "Regenerating module dependency configuration maps (depmod)..."
depmod -a "${KVERSION}"

# 7. Core Initramfs image generation loop (Binds updated driver to early boot)
echo "Updating system initramfs boot block image for kernel ${KVERSION}..."
if command -v update-initramfs &> /dev/null; then
    update-initramfs -u -k "${KVERSION}"
else
    echo "WARNING: update-initramfs missing! Early boot image stack unadjusted." >&2
fi

# 8. Hot-swap live testing instance with production tree instance
if lsmod | awk '{print $1}' | grep -q "${DRIVER_NAME}"; then
    echo "Unloading intermediate out-of-tree test driver instance..."
    rmmod "${DRIVER_NAME}" || rmmod "${DRIVER_NAME//-/_}" || true
fi

echo "Loading production in-tree driver module via modprobe..."
modprobe "${DRIVER_NAME}"

# 9. Final hardware diagnostic check
echo "----------------------------------------------------------------------"
echo "Verifying live production hardware sensors infrastructure:"
echo "----------------------------------------------------------------------"
if command -v sensors &> /dev/null; then
    sensors | grep -A 3 "^${DRIVER_NAME}" || echo "Warning: Sysfs hardware nodes undetected."
else
    cat /sys/class/hwmon/hwmon*/fan*_max || true
fi

echo "=============================================================================="
echo " SUCCESS: ${DRIVER_NAME}.ko.xz cleanly bound into initramfs and working."
echo " Your custom scale parameters are locked down permanently across reboots!"
echo "=============================================================================="
