#!/usr/bin/env bash
# ==============================================================================
# Prepare.sh - Generic Out-of-Tree Build Workspace Staging Suite
# ==============================================================================

set -euo pipefail

# ------------------------------------------------------------------------------
# DRIVER PROFILE CONFIGURATION
# ------------------------------------------------------------------------------
DRIVER_NAME="yogafan"   # Name of your .c file (without extension)
DRIVER_PATH="Files"     # Reference directory containing the source files
KVERSION=$(uname -r)    # Autofilled via system telemetry
# ------------------------------------------------------------------------------

SRC_DIR="src"

echo "=== Staging Workspace for Driver: ${DRIVER_NAME} ==="
echo "Source Location: ${DRIVER_PATH}/${DRIVER_NAME}.c"
echo "Detected Active Kernel Version: ${KVERSION}"

# 1. Purge legacy build workspace
if [ -d "${SRC_DIR}" ]; then
    echo "Purging old '${SRC_DIR}/' directory..."
    rm -rf "${SRC_DIR}"
fi

# 2. Recreate clean flat structure
echo "Creating fresh '${SRC_DIR}/' workspace..."
mkdir -p "${SRC_DIR}"

# 3. Source verification and copy
if [ ! -f "${DRIVER_PATH}/${DRIVER_NAME}.c" ]; then
    echo "ERROR: Source file ${DRIVER_PATH}/${DRIVER_NAME}.c not found!" >&2
    exit 1
fi

echo "Staging latest ${DRIVER_NAME}.c source..."
cp "${DRIVER_PATH}/${DRIVER_NAME}.c" "${SRC_DIR}/"

# 4. Generate dynamic Makefile with protective path quoting
echo "Generating dynamic out-of-tree Makefile for kernel ${KVERSION}..."
cat << EOF > "${SRC_DIR}/Makefile"
obj-m += ${DRIVER_NAME}.o

KVERSION = ${KVERSION}
CUR_DIR  := \$(shell pwd)

all:
	\$(MAKE) -C "/lib/modules/\$(KVERSION)/build" M="\$(CUR_DIR)" modules

clean:
	\$(MAKE) -C "/lib/modules/\$(KVERSION)/build" M="\$(CUR_DIR)" clean
EOF

echo "=============================================================================="
echo " SUCCESS: Workspace staged inside '${SRC_DIR}/' for driver '${DRIVER_NAME}'."
echo "=============================================================================="
