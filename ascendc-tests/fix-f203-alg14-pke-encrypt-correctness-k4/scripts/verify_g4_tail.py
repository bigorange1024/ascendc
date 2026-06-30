#!/usr/bin/env python3
"""
verify_g4_tail.py — G4 tail（INTT→noise→pack）分阶段对拍。

章法：设备 SIM（run_g4_tail_sim，dump 开启）逐阶段落盘 sim_*.bin；本脚本先用
gen_g4_tail_golden.py 生成金标准 golden_*.bin（CPU 全链 max=0，故可信），再按
数据流顺序逐阶段比对，**在第一个对不上的阶段停下并打印首个差异坐标**，从而定位
是 INTT、noise 还是 pack 出错，再据此判断计算 vs tiling/搬运问题。

对拍顺序（数据流）：
    u_time ←INTT(û)      | tr_time ←INTT(tr̂)
    u_noisy ←u_time+e1   | v ←embed(tr_time,m)+e2
    c.bin   ←pack(u_noisy,v)
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parent.parent
OUT = CASE / "output"
HOST_GOLDEN = CASE / "scripts" / "host_golden"

# (阶段名, sim 文件, golden 文件, 期望元素数)
STAGES = [
    ("u_time  (INTT û)", "sim_u_time.bin", "golden_u_time.bin", 1024),
    ("tr_time (INTT tr̂)", "sim_tr_time.bin", "golden_tr_time.bin", 1024),
    ("u_noisy (u+e1)", "sim_u_noisy.bin", "golden_u_noisy.bin", 1024),
    ("v       (tr+e2+μ)", "sim_v.bin", "golden_v.bin", 256),
]


def cmp_stage(name: str, sim_path: Path, gold_path: Path, want: int) -> bool:
    if not sim_path.is_file():
        print(f"[g4_tail] MISS {name}: 设备未产出 {sim_path.name}（kernel 可能在更早阶段崩/未 dump）")
        return False
    if not gold_path.is_file():
        print(f"[g4_tail] MISS {name}: 缺 golden {gold_path.name}（先跑 gen_g4_tail_golden.py）")
        return False
    sim = np.fromfile(sim_path, dtype=np.int32)
    gold = np.fromfile(gold_path, dtype=np.int32)
    if sim.size != want or gold.size != want:
        print(f"[g4_tail] FAIL {name}: size sim={sim.size} gold={gold.size} want={want}")
        return False
    diff = np.abs(sim.astype(np.int64) - gold.astype(np.int64))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        idx = int(diff.argmax())
        nmis = int((diff != 0).sum())
        print(
            f"[g4_tail] FAIL {name}: max={mx} mismatches={nmis}/{want} "
            f"first@{idx} sim={sim[idx]} gold={gold[idx]} (poly={idx // 256} coeff={idx % 256})"
        )
        return False
    print(f"[g4_tail] PASS {name} ({want} coeffs max=0)")
    return True


def main() -> None:
    # 确保 golden 中间量为最新（自包含，禁止 liboqs）
    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gen_g4_tail_golden.py"), str(CASE), str(OUT)],
        check=True,
    )

    all_ok = True
    for name, sim_f, gold_f, want in STAGES:
        ok = cmp_stage(name, OUT / sim_f, OUT / gold_f, want)
        all_ok = all_ok and ok
        if not ok:
            print(f"\n>>> 首个对不上的阶段：{name} —— 在此聚焦排查（计算? tiling? 搬运?）")
            sys.exit(1)

    # 全部中间量一致后，再比最终 c.bin
    c_path = OUT / "c.bin"
    gc_path = OUT / "golden_c.bin"
    if c_path.is_file() and gc_path.is_file():
        c = c_path.read_bytes()
        gc = gc_path.read_bytes()
        if c == gc:
            print(f"[g4_tail] PASS c.bin ({len(c)}B 完全一致)")
        else:
            n = min(len(c), len(gc))
            first = next((i for i in range(n) if c[i] != gc[i]), n)
            print(f"[g4_tail] FAIL c.bin: len sim={len(c)} gold={len(gc)} first@{first}")
            all_ok = False

    if all_ok:
        print("[g4_tail] ALL STAGES PASS —— tail 全链中间量与 golden 一致")
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
