#!/usr/bin/env bash
# Resolve user-facing chip name -> CANN build parameters.
# Usage: source scripts/resolve_device.sh <device>
# Sets: SOC_VERSION, SIM_DEVICE, TIKICPU_LIB_DEVICE, TIKICPU_CPU_LIB, CCEC_ARCH, DEVICE_LABEL

_resolve_device() {
  local raw="${1:-ascend910b4}"
  local key
  key="$(echo "${raw}" | tr '[:upper:]' '[:lower:]' | tr -d ' _-')"

  case "${key}" in
    ascend910|910|910a|ascend910a)
      SOC_VERSION="ascend910"
      SIM_DEVICE="Ascend910A"
      TIKICPU_LIB_DEVICE="Ascend910A"
      TIKICPU_CPU_LIB="Ascend910A"
      CCEC_ARCH="dav-c100"
      DEVICE_LABEL="Ascend910A"
      ;;
    ascend910b|910b)
      SOC_VERSION="Ascend910B1"
      SIM_DEVICE="Ascend910B1"
      TIKICPU_LIB_DEVICE="Ascend910B1"
      TIKICPU_CPU_LIB="Ascend910B1"
      CCEC_ARCH="dav-c220"
      DEVICE_LABEL="Ascend910B (default B1)"
      ;;
    ascend910b1|910b1)
      SOC_VERSION="Ascend910B1"
      SIM_DEVICE="Ascend910B1"
      TIKICPU_LIB_DEVICE="Ascend910B1"
      TIKICPU_CPU_LIB="Ascend910B1"
      CCEC_ARCH="dav-c220"
      DEVICE_LABEL="Ascend910B1"
      ;;
    ascend910b2|910b2)
      SOC_VERSION="Ascend910B2"
      SIM_DEVICE="Ascend910B2"
      TIKICPU_LIB_DEVICE="Ascend910B2"
      TIKICPU_CPU_LIB="Ascend910B1"
      CCEC_ARCH="dav-c220"
      DEVICE_LABEL="Ascend910B2"
      ;;
    ascend910b2c|910b2c)
      SOC_VERSION="Ascend910B2C"
      SIM_DEVICE="Ascend910B2C"
      TIKICPU_LIB_DEVICE="Ascend910B2C"
      TIKICPU_CPU_LIB="Ascend910B1"
      CCEC_ARCH="dav-c220"
      DEVICE_LABEL="Ascend910B2C"
      ;;
    ascend910b3|910b3)
      SOC_VERSION="Ascend910B3"
      SIM_DEVICE="Ascend910B3"
      TIKICPU_LIB_DEVICE="Ascend910B3"
      TIKICPU_CPU_LIB="Ascend910B1"
      CCEC_ARCH="dav-c220"
      DEVICE_LABEL="Ascend910B3"
      ;;
    ascend910b4|910b4)
      SOC_VERSION="Ascend910B4"
      SIM_DEVICE="Ascend910B4"
      TIKICPU_LIB_DEVICE="Ascend910B4"
      TIKICPU_CPU_LIB="Ascend910B1"
      CCEC_ARCH="dav-c220"
      DEVICE_LABEL="Ascend910B4"
      ;;
    ascend910b41|910b41|ascend910b4-1|910b4-1)
      SOC_VERSION="Ascend910B4-1"
      SIM_DEVICE="Ascend910B4"
      TIKICPU_LIB_DEVICE="Ascend910B4"
      TIKICPU_CPU_LIB="Ascend910B1"
      CCEC_ARCH="dav-c220"
      DEVICE_LABEL="Ascend910B4-1 (sim libs: Ascend910B4)"
      ;;
    ascend310p|310p)
      SOC_VERSION="Ascend310P1"
      SIM_DEVICE="Ascend310P1"
      TIKICPU_LIB_DEVICE="Ascend310P1"
      TIKICPU_CPU_LIB="Ascend310P1"
      CCEC_ARCH="dav-m200"
      DEVICE_LABEL="Ascend310P1"
      ;;
    *)
      echo "ERROR: unknown device '${raw}'. Examples: 910B4, ascend910B4, 910, 910B1, 310p" >&2
      return 1
      ;;
  esac

  export SOC_VERSION SIM_DEVICE TIKICPU_LIB_DEVICE TIKICPU_CPU_LIB CCEC_ARCH DEVICE_LABEL
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  _resolve_device "${1:-ascend910b4}"
  echo "SOC_VERSION=${SOC_VERSION}"
  echo "SIM_DEVICE=${SIM_DEVICE}"
  echo "TIKICPU_LIB_DEVICE=${TIKICPU_LIB_DEVICE}"
  echo "CCEC_ARCH=${CCEC_ARCH}"
  echo "DEVICE_LABEL=${DEVICE_LABEL}"
fi
