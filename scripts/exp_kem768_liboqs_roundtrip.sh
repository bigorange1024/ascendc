#!/usr/bin/env bash
# exp_kem768_liboqs_roundtrip.sh — ML-KEM-768 incubating KEM 三件套端到端闭环
#
# 当前口径（2026-07-26）：仓库现有 liboqs fixture/ref helper 仍硬编码 ml_kem_1024
# 尺寸，尚无可直接复用的 liboqs-768 胶水。因此本脚本按 P1 表保留该文件名，
# 但实际执行的是 AscendC-only roundtrip：
#   1. E19 KeyGen：seed_d → ek_kem(1184B), dk_kem(2400B)
#   2. E20 Encaps：读取 E19 ek_kem + 记录下来的 m → c(1088B), K(32B)
#   3. E21 Decaps：读取 E19 dk_kem + E20 c/m/K → K(32B)，并与 E20 K 对拍
#   4. 可选 reject：篡改 c 后喂 E21，要求 K=J(z‖c_bad) 且 K != accept K
#
# 默认算子：incubating k3，无 `-ct` Decaps。
#
# Usage（默认 = CPU×1 + SIM×1；SIM 串行，勿并行多路同目录）：
#   bash scripts/exp_kem768_liboqs_roundtrip.sh
#
# 快速 / 对照（须显式）：
#   SKIP_SIM=1 bash scripts/exp_kem768_liboqs_roundtrip.sh
#   CPU_TRIALS=3 SIM_TRIALS=0 bash scripts/exp_kem768_liboqs_roundtrip.sh
#   DECAPS_DIR=$PWD/examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-ct-k3 \
#     CPU_TRIALS=1 SIM_TRIALS=1 bash scripts/exp_kem768_liboqs_roundtrip.sh
#
# 环境（可选）：SOC_VERSION / CPU_TRIALS / SIM_TRIALS / SKIP_CPU=1 / SKIP_SIM=1
#   KEYGEN_DIR / ENCAPS_DIR / DECAPS_DIR / EXP_KEM768_RT_DIR
#   EXP_KEM768_SKIP_REJECT=1

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOC_VERSION="${SOC_VERSION:-Ascend910B4}"

KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-keygen-k3}"
ENCAPS_DIR="${ENCAPS_DIR:-${REPO_ROOT}/examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-encaps-k3}"
DECAPS_DIR="${DECAPS_DIR:-${REPO_ROOT}/examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-k3}"

CPU_TRIALS="${CPU_TRIALS:-1}"
SIM_TRIALS="${SIM_TRIALS:-1}"

for d in "${KEYGEN_DIR}" "${ENCAPS_DIR}" "${DECAPS_DIR}"; do
    if [[ ! -d "${d}" ]]; then
        echo "[exp_kem768_rt] ERROR: missing ${d}" >&2
        exit 1
    fi
done

if ! [[ "${CPU_TRIALS}" =~ ^[0-9]+$ && "${SIM_TRIALS}" =~ ^[0-9]+$ ]]; then
    echo "[exp_kem768_rt] ERROR: CPU_TRIALS/SIM_TRIALS 须为非负整数" >&2
    exit 1
fi

if [[ "${SKIP_CPU:-0}" == "1" && "${SKIP_SIM:-0}" == "1" ]]; then
    echo "[exp_kem768_rt] ERROR: SKIP_CPU=1 且 SKIP_SIM=1，无事可做" >&2
    exit 1
fi

STAMP="$(date +%Y%m%d_%H%M%S)_$$"
RT_ROOT="${EXP_KEM768_RT_DIR:-${REPO_ROOT}/output/exp_kem768_ascendc_rt/${STAMP}}"
mkdir -p "${RT_ROOT}"

echo "[exp_kem768_rt] REPO=${REPO_ROOT}"
echo "[exp_kem768_rt] mode=AscendC-only (liboqs-768 glue not present)"
echo "[exp_kem768_rt] rt_root=${RT_ROOT}"
echo "[exp_kem768_rt] KEYGEN=${KEYGEN_DIR}"
echo "[exp_kem768_rt] ENCAPS=${ENCAPS_DIR}"
echo "[exp_kem768_rt] DECAPS=${DECAPS_DIR}"
echo "[exp_kem768_rt] CPU_TRIALS=${CPU_TRIALS} SIM_TRIALS=${SIM_TRIALS}"

_require_size() {
    local file="$1"
    local expect="$2"
    local label="$3"
    if [[ ! -f "${file}" ]]; then
        echo "[exp_kem768_rt] ERROR: missing ${label}: ${file}" >&2
        exit 1
    fi
    local got
    got="$(wc -c <"${file}")"
    if [[ "${got}" -ne "${expect}" ]]; then
        echo "[exp_kem768_rt] ERROR: ${label} size got=${got} want=${expect}" >&2
        exit 1
    fi
}

_cmp_file() {
    local label="$1"
    local got="$2"
    local ref="$3"
    python3 - "$label" "$got" "$ref" <<'PY'
import sys
from pathlib import Path

label, got_path, ref_path = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
got = got_path.read_bytes()
ref = ref_path.read_bytes()
if len(got) != len(ref):
    print(f"[exp_kem768_rt] FAIL {label}: size got={len(got)} ref={len(ref)}")
    sys.exit(1)
mx = max((abs(a - b) for a, b in zip(got, ref)), default=0)
if mx != 0:
    print(f"[exp_kem768_rt] FAIL {label}: max={mx}")
    sys.exit(1)
print(f"[exp_kem768_rt] PASS {label}: max=0 ({len(got)} bytes)")
PY
}

_assert_not_equal() {
    local label="$1"
    local a="$2"
    local b="$3"
    python3 - "$label" "$a" "$b" <<'PY'
import sys
from pathlib import Path

label, a_path, b_path = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
a = a_path.read_bytes()
b = b_path.read_bytes()
if len(a) != len(b):
    print(f"[exp_kem768_rt] PASS {label}: size differs ({len(a)} vs {len(b)})")
    sys.exit(0)
mx = max((abs(x - y) for x, y in zip(a, b)), default=0)
if mx == 0:
    print(f"[exp_kem768_rt] FAIL {label}: files are equal")
    sys.exit(1)
print(f"[exp_kem768_rt] PASS {label}: max_diff={mx}")
PY
}

_make_bad_ciphertext() {
    local src="$1"
    local dst="$2"
    python3 - "$src" "$dst" <<'PY'
import sys
from pathlib import Path

src, dst = Path(sys.argv[1]), Path(sys.argv[2])
data = bytearray(src.read_bytes())
if not data:
    raise SystemExit("empty ciphertext")
data[0] ^= 0x01
dst.write_bytes(bytes(data))
PY
}

_run_mode_once() {
    local mode="$1"
    local trial_tag="$2"
    local trial_dir="${RT_ROOT}/${trial_tag}"
    mkdir -p "${trial_dir}"

    echo "[exp_kem768_rt] === ${mode} ${trial_tag}: E19→E20→E21 ==="

    echo "[exp_kem768_rt] Phase 1 KeyGen"
    (
        cd "${KEYGEN_DIR}" && \
            KEM_KG_EXT_SEED=0 \
            KEM_KEYGEN_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _require_size "${KEYGEN_DIR}/output/ek_kem.bin" 1184 "ek_kem"
    _require_size "${KEYGEN_DIR}/output/dk_kem.bin" 2400 "dk_kem"
    cp -f "${KEYGEN_DIR}/output/ek_kem.bin" "${trial_dir}/ek_kem.bin"
    cp -f "${KEYGEN_DIR}/output/dk_kem.bin" "${trial_dir}/dk_kem.bin"

    echo "[exp_kem768_rt] Phase 2 Encaps"
    (
        cd "${ENCAPS_DIR}" && \
            EK_KEM_SRC="${trial_dir}/ek_kem.bin" \
            KEM_ENCAPS_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _require_size "${ENCAPS_DIR}/input/m.bin" 32 "m"
    _require_size "${ENCAPS_DIR}/output/c.bin" 1088 "c"
    _require_size "${ENCAPS_DIR}/output/K.bin" 32 "K_encaps"
    cp -f "${ENCAPS_DIR}/input/m.bin" "${trial_dir}/m.bin"
    cp -f "${ENCAPS_DIR}/output/c.bin" "${trial_dir}/c.bin"
    cp -f "${ENCAPS_DIR}/output/K.bin" "${trial_dir}/K_encaps.bin"

    echo "[exp_kem768_rt] Phase 3 Decaps accept"
    (
        cd "${DECAPS_DIR}" && \
            EK_KEM_SRC="${trial_dir}/ek_kem.bin" \
            DK_KEM_SRC="${trial_dir}/dk_kem.bin" \
            C_SRC="${trial_dir}/c.bin" \
            M_FILE="${trial_dir}/m.bin" \
            K_ENC_SRC="${trial_dir}/K_encaps.bin" \
            KEM_DECAPS_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _require_size "${DECAPS_DIR}/output/K.bin" 32 "K_decaps"
    cp -f "${DECAPS_DIR}/output/K.bin" "${trial_dir}/K_decaps.bin"
    _cmp_file "accept K(encaps)==K(decaps)" "${trial_dir}/K_decaps.bin" "${trial_dir}/K_encaps.bin"

    if [[ "${EXP_KEM768_SKIP_REJECT:-0}" != "1" ]]; then
        echo "[exp_kem768_rt] Phase 4 Decaps reject spot-check"
        _make_bad_ciphertext "${trial_dir}/c.bin" "${trial_dir}/c_bad.bin"
        (
            cd "${DECAPS_DIR}" && \
                EK_KEM_SRC="${trial_dir}/ek_kem.bin" \
                DK_KEM_SRC="${trial_dir}/dk_kem.bin" \
                C_SRC="${trial_dir}/c_bad.bin" \
                KEM_DECAPS_REJECT=1 \
                KEM_DECAPS_VERIFY=1 \
                bash run.sh -r "${mode}" -v "${SOC_VERSION}"
        )
        cp -f "${DECAPS_DIR}/output/K.bin" "${trial_dir}/K_reject.bin"
        _assert_not_equal "reject K != accept K" "${trial_dir}/K_reject.bin" "${trial_dir}/K_encaps.bin"
    else
        echo "[exp_kem768_rt] Phase 4 skipped (EXP_KEM768_SKIP_REJECT=1)"
    fi

    echo "[exp_kem768_rt] ${mode} ${trial_tag} PASS"
}

_run_trials() {
    local mode="$1"
    local n="$2"
    local i
    if [[ "${n}" -le 0 ]]; then
        echo "[exp_kem768_rt] ${mode}: 0 trials (skip)"
        return 0
    fi
    for ((i = 1; i <= n; i++)); do
        if [[ "${mode}" == "sim" ]]; then
            SIM_DIRECT=1 _run_mode_once sim "sim_t${i}"
        else
            _run_mode_once cpu "cpu_t${i}"
        fi
    done
}

if [[ "${SKIP_CPU:-0}" != "1" ]]; then
    _run_trials cpu "${CPU_TRIALS}"
else
    echo "[exp_kem768_rt] SKIP_CPU=1"
fi

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    _run_trials sim "${SIM_TRIALS}"
else
    echo "[exp_kem768_rt] SKIP_SIM=1"
fi

echo "[SUCCESS] ML-KEM-768 incubating AscendC-only roundtrip root=${RT_ROOT}"
echo "[exp_kem768_rt] DECAPS=${DECAPS_DIR} CPU_TRIALS=${CPU_TRIALS} SIM_TRIALS=${SIM_TRIALS}"
