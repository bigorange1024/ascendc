#!/usr/bin/env python3
# coding=utf-8
"""
gen_data_chain_ntt17.py — 行 8–17 链式探针 golden。

Launch 1：SEED_D → Device src[8,256]（真实 Alg.13 CBD，四行 s 互不相同）
Launch 2：src GM → MIX NTT S1–S3（mixPass=5，至 Alg.13 行 17）

NTT golden 链复用 vec-k4-v2 gen_data，但 src 来自 golden_se_sampling（非 tiled FIXED_POLY）。
"""
from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

ROOT = Path(__file__).resolve().parent
VEC_K4_SCRIPTS = ROOT.parent / "pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2" / "scripts"
SE_FIPS203_SE_SCRIPTS = _ascendc_repo_root(Path(__file__).resolve()) / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))
sys.path.insert(0, str(VEC_K4_SCRIPTS))

from golden_se_sampling import build_src  # noqa: E402

import gen_data as ntt_gen  # noqa: E402

SEED_D_DEFAULT = 20260619
MIX_PASS_NTT17 = 5


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"

    input_dir = ROOT / "input"
    output_dir = ROOT / "output"
    input_dir.mkdir(exist_ok=True)
    output_dir.mkdir(exist_ok=True)

    (input_dir / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))

    # 真实 SE 采样 src（四行 s 独立）
    src = build_src(seed_d)
    if src.shape != (8, 256):
        raise SystemExit(f"unexpected src shape {src.shape}")

    uniq = {tuple(row.tolist()) for row in src[:4]}
    if len(uniq) < 4:
        print(f"[WARN] src s rows not all unique (uniq={len(uniq)})")

    src.astype(np.int32).tofile(output_dir / "golden_src.bin")

    rng = np.random.default_rng(ntt_gen.SEED_AHAT)
    a_hat = rng.integers(0, ntt_gen.Q, size=(ntt_gen.K_KK, ntt_gen.N), dtype=np.int32)
    lut = ntt_gen.load_lut_t_i8()
    lut_even = ntt_gen.lut_planar_stacked(lut, even=True)
    lut_odd = ntt_gen.lut_planar_stacked(lut, even=False)

    lut_even.tofile(input_dir / "lut_even_stacked.bin")
    lut_odd.tofile(input_dir / "lut_odd_stacked.bin")
    a_hat.tofile(input_dir / "a_hat.bin")

    payload = np.array([ntt_gen.N, ntt_gen.K_S, MIX_PASS_NTT17], dtype=np.int32)
    buf = np.zeros(16, dtype=np.int32)
    buf[:3] = payload
    buf.tofile(input_dir / "tiling_ntt.bin")

    s_poly = src[: ntt_gen.K_S]
    e_poly = src[ntt_gen.K_S :]
    s0_ref = ntt_gen.encode_2s1e_s0(s_poly, e_poly)
    c_lo_e, c_lo_o, c_hi_e, c_hi_o = ntt_gen.mat_c_tmp_golden(s0_ref, lut)
    mat_c_ref = ntt_gen.pack_mat_c_planar(c_lo_e, c_lo_o, c_hi_e, c_hi_o)
    dst_ref = ntt_gen.golden_dst_from_planar(mat_c_ref)

    s0_ref.tofile(output_dir / "golden_s0.bin")
    mat_c_ref.tofile(output_dir / "golden_mat_c.bin")
    dst_ref.tofile(output_dir / "golden_dst.bin")

    s_hat_dup = dst_ref[ntt_gen.K_S : 2 * ntt_gen.K_S]
    s_hat_ref = dst_ref[: ntt_gen.K_S]
    dup_diff = int(np.abs(s_hat_ref.astype(np.int64) - s_hat_dup.astype(np.int64)).max())

    print(f"[gen_data_chain_ntt17] SEED_D={seed_d} mixPass={MIX_PASS_NTT17}")
    print(f"[gen_data_chain_ntt17] golden_src shape={src.shape} s_uniq={len(uniq)}")
    print(f"[gen_data_chain_ntt17] golden_dst shape={dst_ref.shape} s_hat_dup_diff={dup_diff}")
    print(f"[gen_data_chain_ntt17] a_hat seed={ntt_gen.SEED_AHAT}")


if __name__ == "__main__":
    main()
