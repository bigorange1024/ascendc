#!/usr/bin/env python3
"""
diag_phase_e.py — Alg.21 Decaps Phase-E 逐级污染定位（单 session SIM 排障，2026-07-03）。

背景：单库合并后，SIM 单 session 下 Phase-D 输出 m'/K'/coins max=0，但 Phase-E 末尾
      c' vs c max=244（FO 误拒 → K max=216）。2-session（aclFinalize 后 fresh encrypt）
      则 c' max=0。需定位污染沿 Phase-E 数据流的**第一个出错级**。

判别逻辑：
  - a_hat 仅依赖 ek 的 ρ（ek[1536:1568]），**完全不依赖 Phase-D**，且为纯 SampleNTT。
    若单 session dump 的 a_hat 已与自包含 golden 有差 → 铁证 CAModel 在同 session 第二批
    launch 上把纯函数核算错/装载错（与「上游数据被污染」无关）。
  - re 依赖 coins（Phase-D 输出，已 max=0，且已重 H2D）。若 a_hat 对而 re 错 → coins 传递
    /prep_re 在此 session 上下文出错。
  - 逐级向后（r_hat/t_hat/...）留待需要时扩展；本脚本先给出 a_hat / re 定性结论。

输入（均由 main 在 KEM_DECAPS_DEBUG=1 下 dump 到 output/）：
  dbg_ek.bin(1568B)、dbg_coins.bin(32B)、dbg_a_hat.bin(int32 16*256)、
  dbg_re.bin(int32 (4+4)*256+256)、dbg_t_hat.bin、dbg_r_hat.bin ...
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE / "host_golden"))
import gate_g1  # noqa: E402  (build_a_hat / build_re / K / N)
import gate_g2  # noqa: E402  (ntt_r_hat_golden)
import gate_g3  # noqa: E402  (decode_t_hat_from_ek / golden_u_hat / golden_tr_hat)

K = gate_g1.K
N = gate_g1.N


def _max_abs_diff_i32(dev_bin: Path, golden: np.ndarray) -> tuple[int, int, int]:
    """返回 (max_abs_diff, dev_max_abs, n_mismatch)。dev_bin 为 int32 小端。"""
    dev = np.frombuffer(dev_bin.read_bytes(), dtype=np.int32)
    g = golden.astype(np.int32).ravel()
    n = min(dev.size, g.size)
    if dev.size != g.size:
        print(f"  [warn] size mismatch dev={dev.size} golden={g.size}")
    d = np.abs(dev[:n].astype(np.int64) - g[:n].astype(np.int64))
    return int(d.max()), int(np.abs(dev[:n]).max()), int((d != 0).sum())


def main() -> None:
    root = _HERE.parent
    out = root / "output"

    ek = (out / "dbg_ek.bin").read_bytes()
    if len(ek) != 1568:
        raise SystemExit(f"bad dbg_ek.bin len={len(ek)}")
    rho = ek[1536:1568]
    coins = (out / "dbg_coins.bin").read_bytes()
    if len(coins) != 32:
        raise SystemExit(f"bad dbg_coins.bin len={len(coins)}")

    # golden（自包含，禁 liboqs）
    g_a_hat = gate_g1.build_a_hat(rho)              # int32 [K*K*N]
    g_r, g_e1, g_e2 = gate_g1.build_re(coins)       # [K,N],[K,N],[N]
    g_re = np.concatenate([g_r.ravel(), g_e1.ravel(), g_e2.ravel()]).astype(np.int32)

    print("[diag_phase_e] 单 session Phase-E 逐级 vs 自包含 golden")
    print(f"  rho[:4]={rho[:4].hex()}  coins[:4]={coins[:4].hex()}")

    # a_hat：Phase-D-independent 关键判别
    md, dmax, nmis = _max_abs_diff_i32(out / "dbg_a_hat.bin", g_a_hat)
    print(f"  a_hat  : max_diff={md:6d}  dev_max={dmax:6d}  n_mismatch={nmis}")
    a_hat_ok = md == 0

    # re：依赖 coins
    md_re, dmax_re, nmis_re = _max_abs_diff_i32(out / "dbg_re.bin", g_re)
    print(f"  re     : max_diff={md_re:6d}  dev_max={dmax_re:6d}  n_mismatch={nmis_re}")
    re_ok = md_re == 0

    # t_hat：ByteDecode₁₂(ek[0:1536])，纯解码，不依赖 Phase-D 也不依赖 a_hat/re。
    g_t_hat = gate_g3.decode_t_hat_from_ek(ek[:1536])
    md_th, dmax_th, nmis_th = _max_abs_diff_i32(out / "dbg_t_hat.bin", g_t_hat)
    print(f"  t_hat  : max_diff={md_th:6d}  dev_max={dmax_th:6d}  n_mismatch={nmis_th}")
    t_hat_ok = md_th == 0

    # r_hat：ntt_r(re[0:K])。re golden 已与 dev 一致，故此 golden 为 ntt_r 的期望输出。
    g_r = g_re[: K * N].reshape(K, N)
    g_r_hat = gate_g2.ntt_r_hat_golden(g_r).reshape(-1)
    md_rh, dmax_rh, nmis_rh = _max_abs_diff_i32(out / "dbg_r_hat.bin", g_r_hat)
    print(f"  r_hat  : max_diff={md_rh:6d}  dev_max={dmax_rh:6d}  n_mismatch={nmis_rh}")
    r_hat_ok = md_rh == 0

    # u_tr = at_r5 输出：前 K*N 为 u_hat(Âᵀ·r̂)，后 N 为 tr_hat(t̂·r̂)。
    # golden 用 golden 输入(a_hat/t_hat/r_hat)算，与 dev 输出比 → 定位 at_r5 本身是否算错。
    u_tr_dev = np.frombuffer((out / "dbg_u_tr.bin").read_bytes(), dtype=np.int32)
    g_u_hat = gate_g3.golden_u_hat(g_a_hat, g_r_hat)          # [K*N]
    g_tr_hat = gate_g3.golden_tr_hat(g_t_hat, g_r_hat)        # [N]
    du = np.abs(u_tr_dev[: K * N].astype(np.int64) - g_u_hat.astype(np.int64))
    dtr = np.abs(u_tr_dev[K * N : K * N + N].astype(np.int64) - g_tr_hat.astype(np.int64))
    md_u, md_tr = int(du.max()), int(dtr.max())
    print(f"  u_hat  : max_diff={md_u:6d}  dev_max={int(np.abs(u_tr_dev[:K*N]).max()):6d}")
    print(f"  tr_hat : max_diff={md_tr:6d}  dev_max={int(np.abs(u_tr_dev[K*N:K*N+N]).max()):6d}")

    print("\n[diag_phase_e] 判读（第一个出错级即污染注入点）：")
    if not a_hat_ok:
        print("  ✗ a_hat 单 session 已错 → CAModel 同 session 第二批 launch 核算/装载错误。")
    elif not re_ok:
        print("  ✗ a_hat 对但 re 错 → coins 传递 / prep_re 在此 session 上下文出错。")
    elif not t_hat_ok:
        print("  ✗ t_hat 错 → decode_t_hat 在此 session 解码/装载错误（纯解码，不依赖上游）。")
    elif not r_hat_ok:
        print("  ✗ r_hat 错（re 对） → **ntt_r 是第一个出错级**：输入正确但该核在此 session 算错/搬运错。")
    elif md_u != 0 or md_tr != 0:
        print("  ✗ u_hat/tr_hat 错（a_hat/re/r_hat/t_hat 均对） → **at_r5(MMAD) 是第一个出错级**。")
    else:
        print("  ✓ 至 at_r5 全对 → 污染在 intt / g4_noise / pack 之一；需继续向后逐级比对。")


if __name__ == "__main__":
    main()
