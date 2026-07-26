# pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4

**晋级**：2026-07-07 自 `fix-` 重命名为 `pass-`（SIM 单 launch 全量验收；CPU 三 launch 部分对照）。

**状态（分平台）**：

| 平台 | 判定 | 说明 |
|------|------|------|
| **SIM 默认单 launch** | **完成** | 行 2/18/19/21（不含 μ）全在设备 `f203_encrypt_l18_l19` 内验收 |
| **CPU 三 launch** | **部分对照** | tikicpu MIX 串行 **不得** 调单 launch（死锁）；仅 û/u 子集 |
| **全探针（严格双模式同口径）** | **有条件完成** | 生产验收面 = SIM；CPU 为文档化分叉例外，见 [AscendC-CPU与SIM实现分叉开发指南.md](../../docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) §2 |

**根因**：`f203_encrypt_l18_l19` 为长 MIX FSM（AIC↔AIV CrossCore）。tikicpu **按核型串行**执行 → AIC 先 `WaitFlag` 而 AIV 未 `Set` → **永久死锁**。故 CPU host **固定** `RunCpuThreeLaunch`（`ntt_y` → `at_jp` → `intt_e1`），**禁止** launch 融合核。

**kP=5 + INTT pad→8（2026-07-07）**（**仅 SIM 单 launch**）：统一内积 `uTr[5]`（û[0..3]+tr̂[4]）→ INTT batch k=8 → `u←INTT(û)+e₁`、`v←INTT(tr̂)+e₂`。UB 分片：AIV0 `[uTr0,uTr1,uTr4,0]`，AIV1 `[uTr2,uTr3,0,0]`。

**内积**：SIM **Alg11 向量 basemul**（`ALG11_IMPL=1` `B2` `MEM_OPS=1`）；`t_hat` 行 2 驻留 UB（`F203_BYTE_DECODE12_IMPL=0` 标量默认）。

## 已实现 vs 待办（不含 μ）

| 功能 | SIM 单 launch | CPU 三 launch | 说明 |
|------|---------------|---------------|------|
| 行 16–17 `ŷ←NTT(y)` | ✓ 设备 | ✓ 设备 | MIX NTT k=4 |
| 行 2 `t̂←ByteDecode₁₂(ek)` | ✓ 设备 AIV | — | CPU 无设备 decode；行 2 不参与 CPU 对拍 |
| 行 18 `û` + `tr̂`（kP=5） | ✓ 设备 | 部分 | CPU 仅 4 列 û（`at_jp`）；无统一 `uTr[5]` |
| 行 19 `u←INTT(û)+e₁` | ✓ 设备 k=8 | ✓ 设备 k=4 | CPU `intt_e1` 无 pad→8 |
| 行 21 `v←INTT(tr̂)+e₂` | ✓ 设备 | — | CPU 不写 `v.bin`；`run.sh` 跳过 v/u_tr |
| 与 prep 拼接 | 未实现 | 未实现 | 独立 `gen_data` |
| Compress / ByteEncode `c` | 范围外 | 范围外 | 后续 stable encrypt |

## 验收

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4          # 部分：y_hat + u_ntt + u
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # 全量：+ u_tr + v（默认单 launch）
```

| 模式 | Host | Launch | 对拍 | 覆盖行 |
|------|------|--------|------|--------|
| **SIM 默认** | `RunSimFusedSingleLaunch` | 1 | y_hat, u_ntt, **u_tr**, u, **v** | 2, 16–17, 18, 19, 21 |
| CPU | `RunCpuThreeLaunch` | 3 | y_hat, u_ntt, u | 16–17, 18（û）, 19（u） |
| SIM 调试 | `RunSimThreeLaunch` | 3 | 同 CPU | 分段调试，非生产 |

`run.sh` 在 CPU（及 `phased_launch`）下 **跳过** `u_tr`/`v` 对拍（`skip_kp5`），避免误报全链通过。

## 代码

- `compute/f203_encrypt_l18_l19_kernel.cpp` — **SIM 专用**单 launch（NTT→内积 uTr→INTT k=8→+e₁/+e₂）
- `compute/f203_encrypt_intt_stage.hpp` — INTT k=8 AIV 类（loRow 用 kInttBatch）
- `compute/f203_encrypt_at_jp_vec.hpp` — `ProcessToUbMaybeTrHat` 统一 uTr pad-8
- `compute/f203_l18_l19_tiling.h` — kInttBatch=8、文件尺寸
- `scripts/gen_data.py` — golden u_tr、v、e2
