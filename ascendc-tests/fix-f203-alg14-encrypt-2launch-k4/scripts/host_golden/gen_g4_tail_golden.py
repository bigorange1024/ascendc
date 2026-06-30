#!/usr/bin/env python3
"""
gen_g4_tail_golden.py — Alg.14 G4 tail（INTT→noise→pack）分阶段 golden。

章法：CPU 全链已 max=0，故 Python 参考（golden_c.py / f203_ref_common.py）即金标准。
本脚本把 tail 每个关键阶段的中间量单独落盘，供设备 SIM 逐阶段对拍：

    u_hat ─INTT→ u_time ─(+e1)→ u_noisy ┐
                                          ├─pack→ c
    tr_hat ─INTT→ tr_time ─(+e2,+μ)→ v  ┘

落盘（全部 int32，与设备 D2H dump 同布局）：
    golden_u_hat.bin   [K*N]  INTT 输入（NTT 域 û，来自 G3）
    golden_tr_hat.bin  [N]    INTT 输入（NTT 域 tr̂）
    golden_u_time.bin  [K*N]  INTT(û)，加噪前（对 sim_u_time）
    golden_tr_time.bin [K*N]  INTT(tr_pad)，加噪前；仅 poly0 有效（对 sim_tr_time）
    golden_u_noisy.bin [K*N]  u_time + e1 mod q（对 sim_u_noisy / pack 的 u 入参）
    golden_v.bin       [N]    embed(tr_time,m) + e2 mod q（对 sim_v / pack 的 v 入参）
    golden_e1.bin      [K*N]  noise 输入（CBD e1）
    golden_e2.bin      [N]    noise 输入（CBD e2）

输入：case_dir/input/{ek_pke.bin, m.bin, coins.bin}（禁止 liboqs，全部自包含）。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from f203_ref_common import K, N, Q, embed_message, stage123_transform
from golden_c import build_a_hat, build_re, decode_t_hat, golden_tr_hat, golden_u_hat

EK_T_BYTES = 1536


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <case_dir> <out_dir>", file=sys.stderr)
        sys.exit(1)
    case_dir = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    ek = (case_dir / "input" / "ek_pke.bin").read_bytes()
    m = (case_dir / "input" / "m.bin").read_bytes()
    coins = (case_dir / "input" / "coins.bin").read_bytes()
    if len(ek) != 1568 or len(m) != 32 or len(coins) != 32:
        raise SystemExit(f"bad input sizes ek={len(ek)} m={len(m)} coins={len(coins)}")

    # —— 复刻 golden_c.golden_encrypt 的前半段（NTT 域 û/tr̂）——
    rho = ek[1536:1568]
    t_hat = decode_t_hat(ek[:EK_T_BYTES])
    a_hat = build_a_hat(rho)
    r, e1, e2 = build_re(coins)
    r_hat = stage123_transform(r, "ntt")
    u_hat = golden_u_hat(a_hat, r_hat)          # (K, N)
    tr_hat = golden_tr_hat(t_hat, r_hat)        # (N,)

    # —— tail：INTT 还原到时域 ——
    u_time = stage123_transform(u_hat, "intt")  # (K, N)
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr_time = stage123_transform(tr_pad, "intt")  # (K, N)，仅 poly0 有效

    # —— noise + 消息嵌入 ——
    u_noisy = (u_time.astype(np.int64) + e1.astype(np.int64)) % Q
    u_noisy = u_noisy.astype(np.int32)
    v = embed_message(tr_time[0], m)
    v = ((v.astype(np.int64) + e2.astype(np.int64)) % Q).astype(np.int32)

    # —— 落盘（int32，flatten 与设备 GM 一致）——
    u_hat.astype(np.int32).reshape(-1).tofile(out_dir / "golden_u_hat.bin")
    tr_hat.astype(np.int32).reshape(-1).tofile(out_dir / "golden_tr_hat.bin")
    u_time.astype(np.int32).reshape(-1).tofile(out_dir / "golden_u_time.bin")
    tr_time.astype(np.int32).reshape(-1).tofile(out_dir / "golden_tr_time.bin")
    u_noisy.reshape(-1).tofile(out_dir / "golden_u_noisy.bin")
    v.reshape(-1).tofile(out_dir / "golden_v.bin")
    e1.astype(np.int32).reshape(-1).tofile(out_dir / "golden_e1.bin")
    e2.astype(np.int32).reshape(-1).tofile(out_dir / "golden_e2.bin")
    print(
        "[gen_g4_tail] OK "
        f"u_hat={u_hat.size} u_time={u_time.size} tr_time={tr_time.size} "
        f"u_noisy={u_noisy.size} v={v.size}"
    )


if __name__ == "__main__":
    main()
