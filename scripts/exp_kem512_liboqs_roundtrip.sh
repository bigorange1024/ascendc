#!/usr/bin/env bash
# exp_kem512_liboqs_roundtrip.sh — ML-KEM-512 incubating KEM 三件套端到端闭环
#
# 两种模式：
#   默认 AscendC-only（历史口径）：E19→E20→E21 自洽 + 可选 reject
#   USE_LIBOQS=1：真 liboqs-512 交叉（委托 stable_kem_liboqs_roundtrip.sh + MLKEM_PARAM=512）
#
# Usage：
#   bash scripts/exp_kem512_liboqs_roundtrip.sh
#   USE_LIBOQS=1 bash scripts/exp_kem512_liboqs_roundtrip.sh
#   SKIP_SIM=1 bash scripts/exp_kem512_liboqs_roundtrip.sh
#
# 环境：SOC_VERSION / CPU_TRIALS / SIM_TRIALS / SKIP_CPU / SKIP_SIM
#       KEYGEN_DIR / ENCAPS_DIR / DECAPS_DIR / EXP_KEM512_RT_DIR
#       EXP_KEM512_SKIP_REJECT=1 / USE_LIBOQS=1

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

KEYGEN_DIR="${KEYGEN_DIR:-${REPO_ROOT}/examples/incubating/ml-kem/ml-kem-512/exp-fips203-mlkem-kem-keygen-k2}"
ENCAPS_DIR="${ENCAPS_DIR:-${REPO_ROOT}/examples/incubating/ml-kem/ml-kem-512/exp-fips203-mlkem-kem-encaps-k2}"
DECAPS_DIR="${DECAPS_DIR:-${REPO_ROOT}/examples/incubating/ml-kem/ml-kem-512/exp-fips203-mlkem-kem-decaps-k2}"

if [[ "${USE_LIBOQS:-0}" == "1" ]]; then
    echo "[exp_kem512_rt] mode=liboqs-cross MLKEM_PARAM=512"
    export MLKEM_PARAM=512
    export KEYGEN_DIR ENCAPS_DIR DECAPS_DIR
    export LIBOQS_KEM_VS_SKIP_REJECT="${EXP_KEM512_SKIP_REJECT:-${LIBOQS_KEM_VS_SKIP_REJECT:-0}}"
    export STABLE_KEM_RT_DIR="${EXP_KEM512_RT_DIR:-${REPO_ROOT}/output/exp_kem512_liboqs_rt}"
    # 与历史脚本默认对齐：未显式设则 CPU×1+SIM×1
    export CPU_TRIALS="${CPU_TRIALS:-1}"
    export SIM_TRIALS="${SIM_TRIALS:-1}"
    exec bash "${REPO_ROOT}/scripts/stable_kem_liboqs_roundtrip.sh"
fi

# ---------- AscendC-only（以下保持原逻辑）----------
SOC_VERSION="${SOC_VERSION:-Ascend910B4}"
CPU_TRIALS="${CPU_TRIALS:-1}"
SIM_TRIALS="${SIM_TRIALS:-1}"

for d in "${KEYGEN_DIR}" "${ENCAPS_DIR}" "${DECAPS_DIR}"; do
    if [[ ! -d "${d}" ]]; then
        echo "[exp_kem512_rt] ERROR: missing ${d}" >&2
        exit 1
    fi
done

if ! [[ "${CPU_TRIALS}" =~ ^[0-9]+$ && "${SIM_TRIALS}" =~ ^[0-9]+$ ]]; then
    echo "[exp_kem512_rt] ERROR: CPU_TRIALS/SIM_TRIALS 须为非负整数" >&2
    exit 1
fi

if [[ "${SKIP_CPU:-0}" == "1" && "${SKIP_SIM:-0}" == "1" ]]; then
    echo "[exp_kem512_rt] ERROR: SKIP_CPU=1 且 SKIP_SIM=1，无事可做" >&2
    exit 1
fi

STAMP="$(date +%Y%m%d_%H%M%S)_$$"
RT_ROOT="${EXP_KEM512_RT_DIR:-${REPO_ROOT}/output/exp_kem512_ascendc_rt/${STAMP}}"
mkdir -p "${RT_ROOT}"

echo "[exp_kem512_rt] REPO=${REPO_ROOT}"
echo "[exp_kem512_rt] mode=AscendC-only (set USE_LIBOQS=1 for true liboqs-512 cross)"
echo "[exp_kem512_rt] rt_root=${RT_ROOT}"
echo "[exp_kem512_rt] KEYGEN=${KEYGEN_DIR}"
echo "[exp_kem512_rt] ENCAPS=${ENCAPS_DIR}"
echo "[exp_kem512_rt] DECAPS=${DECAPS_DIR}"
echo "[exp_kem512_rt] CPU_TRIALS=${CPU_TRIALS} SIM_TRIALS=${SIM_TRIALS}"

_require_size() {
    local file="$1"
    local expect="$2"
    local label="$3"
    if [[ ! -f "${file}" ]]; then
        echo "[exp_kem512_rt] ERROR: missing ${label}: ${file}" >&2
        exit 1
    fi
    local got
    got="$(wc -c <"${file}")"
    if [[ "${got}" -ne "${expect}" ]]; then
        echo "[exp_kem512_rt] ERROR: ${label} size got=${got} want=${expect}" >&2
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
    print(f"[exp_kem512_rt] FAIL {label}: size got={len(got)} ref={len(ref)}")
    sys.exit(1)
mx = max((abs(a - b) for a, b in zip(got, ref)), default=0)
if mx != 0:
    print(f"[exp_kem512_rt] FAIL {label}: max={mx}")
    sys.exit(1)
print(f"[exp_kem512_rt] PASS {label}: max=0 ({len(got)} bytes)")
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
    print(f"[exp_kem512_rt] PASS {label}: size differs ({len(a)} vs {len(b)})")
    sys.exit(0)
mx = max((abs(x - y) for x, y in zip(a, b)), default=0)
if mx == 0:
    print(f"[exp_kem512_rt] FAIL {label}: files are equal")
    sys.exit(1)
print(f"[exp_kem512_rt] PASS {label}: max_diff={mx}")
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

    echo "[exp_kem512_rt] === ${mode} ${trial_tag}: E19→E20→E21 ==="

    echo "[exp_kem512_rt] Phase 1 KeyGen"
    (
        cd "${KEYGEN_DIR}" && \
            KEM_KG_EXT_SEED=0 \
            KEM_KEYGEN_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _require_size "${KEYGEN_DIR}/output/ek_kem.bin" 800 "ek_kem"
    _require_size "${KEYGEN_DIR}/output/dk_kem.bin" 1632 "dk_kem"
    cp -f "${KEYGEN_DIR}/output/ek_kem.bin" "${trial_dir}/ek_kem.bin"
    cp -f "${KEYGEN_DIR}/output/dk_kem.bin" "${trial_dir}/dk_kem.bin"

    echo "[exp_kem512_rt] Phase 2 Encaps"
    (
        cd "${ENCAPS_DIR}" && \
            EK_KEM_SRC="${trial_dir}/ek_kem.bin" \
            KEM_ENCAPS_VERIFY=0 \
            bash run.sh -r "${mode}" -v "${SOC_VERSION}"
    )
    _require_size "${ENCAPS_DIR}/input/m.bin" 32 "m"
    _require_size "${ENCAPS_DIR}/output/c.bin" 768 "c"
    _require_size "${ENCAPS_DIR}/output/K.bin" 32 "K_encaps"
    cp -f "${ENCAPS_DIR}/input/m.bin" "${trial_dir}/m.bin"
    cp -f "${ENCAPS_DIR}/output/c.bin" "${trial_dir}/c.bin"
    cp -f "${ENCAPS_DIR}/output/K.bin" "${trial_dir}/K_encaps.bin"

    echo "[exp_kem512_rt] Phase 3 Decaps accept"
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

    if [[ "${EXP_KEM512_SKIP_REJECT:-0}" != "1" ]]; then
        echo "[exp_kem512_rt] Phase 4 Decaps reject spot-check"
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
        echo "[exp_kem512_rt] Phase 4 skipped (EXP_KEM512_SKIP_REJECT=1)"
    fi

    echo "[exp_kem512_rt] ${mode} ${trial_tag} PASS"
}

_run_trials() {
    local mode="$1"
    local n="$2"
    local i
    if [[ "${n}" -le 0 ]]; then
        echo "[exp_kem512_rt] ${mode}: 0 trials (skip)"
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
    echo "[exp_kem512_rt] SKIP_CPU=1"
fi

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    _run_trials sim "${SIM_TRIALS}"
else
    echo "[exp_kem512_rt] SKIP_SIM=1"
fi

echo "[SUCCESS] ML-KEM-512 incubating AscendC-only roundtrip root=${RT_ROOT}"
echo "[exp_kem512_rt] DECAPS=${DECAPS_DIR} CPU_TRIALS=${CPU_TRIALS} SIM_TRIALS=${SIM_TRIALS}"
