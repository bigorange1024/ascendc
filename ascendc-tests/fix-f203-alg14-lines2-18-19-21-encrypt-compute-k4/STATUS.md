# fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4

**状态**：**完成** — 行 18–19 可行性 CPU + SIM（默认 3 launch；`F203_FEAS_FUSED=1` 单 launch SIM 亦 max=0）。

## 可行性结论（2026-07-06）

| 段 | Launch | 核型 | 验证 |
|----|--------|------|------|
| 行 18 `ŷ←NTT(y)` | `f203_encrypt_ntt_y` | MIX 1×AIC+2×AIV | `y_hat` max=0 |
| 行 19 NTT 域 `û←Âᵀ∘ŷ` | `f203_encrypt_at_jp` | AIV×2 halfrows | `u_ntt` max=0 |
| 行 19 时域 `u←INTT(û)+e₁` | `f203_encrypt_intt_e1` | MIX INTT + 分片加噪 | `u` max=0 |

**同步设计（已证实）**：

1. 行 18 末：双 AIV 分片写 `y_hat` GM → **不显式拷到单 AIV**；下行 19 从 GM 读全量 ŷ。
2. 行间：`host` 顺序 launch（等价 `aclrtSynchronizeStream`）即 SYNC-ŷ。
3. 行 19 内积：双 AIV 各算 p∈{0,1}/{2,3}，读 GM 全量 ŷ[0..3]。
4. INTT：独立 MIX launch；AIC 无需等待内积（已用 launch 边界隔开）。

**SIM 要点**：

- host 用 `ACLRT_LAUNCH_KERNEL` + **pinned host** `TilingData*`（非 device tiling）。
- INTT 阶段 LUT 置于 ws offset 0（三 launch 路径）；`LUT_INTT_*` 大偏移仅保留给单 launch 融合试验。

**单 launch `f203_encrypt_l18_l19`（`F203_FEAS_FUSED=1`）**：

| 项 | 结论 |
|----|------|
| CPU | 不支持（tikicpu 串行先跑 AIC 会死锁） |
| SIM（2026-07-06） | **完成**：`y_hat`/`u_ntt`/`u` max=0，~130s |
| 根因（u 曾仅 =e₁） | 同 kernel 标量写 `uNtt` 后 MTE `DataCopy` 读 GM 不可见（SIM） |
| 修复 | **û 驻留 UB** → `ProcessFromLocal` split；`DataCopy` 落盘 `u_ntt`；INTT 握手 flag 1/3 + GATE 4/8 |

## 验收

```bash
cd ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # 默认 KERNEL_COMPUTE_BUDGET_SEC=180
```

默认 **3 launch**（`main.cpp` `RunPhasedCpu` / `RunPhasedSim`）；非 `F203_FEAS_FUSED`。

## 代码

- `compute/f203_encrypt_ntt_y_kernel.cpp` — 行 18
- `compute/f203_encrypt_at_jp_kernel.cpp` — 行 19 内积
- `compute/f203_encrypt_intt_e1_kernel.cpp` — INTT + e₁
- `compute/f203_encrypt_l18_l19_kernel.cpp` — 单 launch 试验（SIM）
- `scripts/gen_data.py` — golden
