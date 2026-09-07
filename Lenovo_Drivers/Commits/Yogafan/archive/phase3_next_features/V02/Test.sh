#!/usr/bin/env bash
# ==============================================================================
# Test.sh - Local Compilation, Runtime Swap, and Live Verification
# ==============================================================================

set -euo pipefail

# ------------------------------------------------------------------------------
# DRIVER PROFILE CONFIGURATION
# ------------------------------------------------------------------------------
DRIVER_NAME="yogafan"
DRIVER_PATH="Files"
# ------------------------------------------------------------------------------

# Root validation check - Preserves exact execution paths (PWD) across sudo
if [ "$EUID" -ne 0 ]; then
    echo "----------------------------------------------------------------------"
    echo "NOTICE: Superuser privileges required for hot-swapping live modules."
    echo "----------------------------------------------------------------------"
    exec sudo -E env "PWD=$(pwd)" "$0" "$@"
fi

SRC_DIR="src"
TARGET_KO="${SRC_DIR}/${DRIVER_NAME}.ko"

echo "=== Starting Verification Sequence for: ${DRIVER_NAME} ==="

# 1. Environment Verification
if [ ! -d "${SRC_DIR}" ] || [ ! -f "${SRC_DIR}/Makefile" ]; then
    echo "ERROR: Local build layout missing. Execute ./Prepare.sh first." >&2
    exit 1
fi

# 2. Compile execution
echo "Compiling out-of-tree module components..."
make -C "${SRC_DIR}"

# 3. Artifact verification
if [ ! -f "${TARGET_KO}" ]; then
    echo "ERROR: Compilation finished but binary '${TARGET_KO}' is missing!" >&2
    exit 1
fi
echo "Module built successfully at: ${TARGET_KO}"

# 4. Seamless automated module removal (catches underscores/hyphen anomalies)
if lsmod | awk '{print $1}' | grep -q "${DRIVER_NAME}"; then
    echo "Active runtime driver detected. Executing hot-removal..."
    rmmod "${DRIVER_NAME}" || rmmod "${DRIVER_NAME//-/_}" || true
fi

# 5. Insert new test driver
echo "Injecting updated test module binary: ${TARGET_KO}..."
insmod "${TARGET_KO}"

# 6. Telemetry logging checks
echo "Recent kernel log context streams:"
echo "----------------------------------------------------------------------"
dmesg | grep -i "${DRIVER_NAME}" | tail -n 5
echo "----------------------------------------------------------------------"

if command -v sensors &> /dev/null; then
    echo "Verifying sysfs register exposure via sensors tool:"
    sensors | grep -A 3 "^${DRIVER_NAME}" || echo "Warning: Active sysfs nodes not exposed yet."
fi

echo "=============================================================================="
echo " SUCCESS: Runtime test instance running. Check fan metrics profile above."
echo "=============================================================================="
