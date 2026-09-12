# AV01-single-aiv-mlkem-ntt · STATUS

> 日期：2026-09-12  
> 结论：**PASS**（CPU + SIM 正确性）；SIM 性能相对 EN01 **可作基础能力**（单 poly 对位有竞争力；polyvec 批处理仍逊 Cube）

---

## 1. 目标

确认 `thirdparty/ntt` 是否支持 **单 AIV、单用例 ML-KEM**；抽取进工程；SIM 测 tick，与既有 cann-ntt（EN01）对比；评估是否作为基础能力并考虑重写 Encrypt/Encaps。

## 2. 能力边界（源包）

| 项 | 结论 |
|----|------|
| 单 AIV | 是（`KERNEL_TYPE_AIV_ONLY`，无 Cube） |
| 单 poly / 单用例 | 是（n=256，一次 launch 一条 poly） |
| ML-KEM | 是（q=3329，7 层；DSA 分支本探针已剥除） |
| 原包构建 | 仅 `RUN_MODE=npu`；本探针改用仓内 cpu/SIM KernelLaunch 壳 |

## 3. 迁入列表

| 路径 | 来源 |
|------|------|
| `single_aiv_mlkem_ntt.cpp` | `thirdparty/ntt/src/ntt_kernel.cpp`（仅 Kem=true；中文注释） |
| `generated/{tables,constants}.hpp` | 抽取 KEM 根表/置换表 |
| `include/reference.hpp` | 独立 DFT Oracle（host 对拍参考） |
| `cmake/*` `CMakeLists.txt` `run.sh` `data_utils.h` | 壳对齐 `bricks/RB-T05-*` |

未改 `thirdparty/`；未开 INTT / Encrypt；禁 `-r npu`。

## 4. 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | golden **match**；wall≈1.0s |
| SIM | 0 | golden **match**；Total tick **7871**；wall≈1.84s；stray 收拢 `sim_log/` |

日志：`/opt/cursor/artifacts/aiv-ntt-sim-compare/AV01-sim.log`

## 5. 与 EN01 cann-ntt 对比（SIM）

| 用例 | 形态 | polys/launch | Total tick | golden |
|------|------|-------------:|-----------:|--------|
| **AV01** | 单 AIV + Gather 蝶形 | **1** | **7871** | PASS |
| EN01 `NTT_BENCH=1` | MIX Cube | 1 | 9343 | **SOFT_FAIL**（输出全 0；tiling/ws 疑不适配 bench=1） |
| EN01 默认 `NTT_BENCH=4` | MIX Cube | 4 | **11029** | PASS（≈2757 tick/poly 摊销） |

日志：`EN01-bench1-sim.log`、`EN01-bench4-sim.log`；综述见同目录 `COMPARE.md`。

### 解读

- **单 poly 一次 launch**：AV01 **7871** ≤ EN01 bench=1 的 9343（后者正确性未过，仅作 tick 参考）。
- **polyvec 批（Encrypt 常态 k=4）**：EN01 一发 4 poly 仅 11029；若 AV01 串 4 发 ≈ 31k tick，**批处理 Cube 仍明显更优**。
- 作者 NPU 宣称 ML-KEM≈6.1µs：**SIM tick ≠ 真机 µs**；上机需另授。

## 6. 基础能力 / Encrypt·Encaps 建议

| 建议 | 说明 |
|------|------|
| **采纳为单 poly AIV NTT 积木** | 正确性已过；无 Cube/无 CrossCore；适合 AIV-heavy 流水或避 Cube 同步的段 |
| **不要直接整段替换 EN13 cann-ntt Encrypt** | 全链 NTT/INTT 次数×k，Cube 批处理仍是吞吐主力 |
| **若重写 Encrypt/Encaps** | 宜「AIV NTT 积木 + 仍 AIV 的 Prep/Pack」试点；或把 AV01 **扩成 polyvec 批**后再挑战全链；需单独立项，非本刀范围 |

## 7. 已锁参数

`n=256` `q=3329` `blockDim=1` `KERNEL_TYPE_AIV_ONLY`；禁擅自改根表/置换表；禁 `-r npu`。

## 8. 一条教训

EN01 的 `NTT_BENCH` 不是自由旋钮：bench=1 在本壳上出现全 0 输出；对比应用 **默认批** + 明示「单 poly 对位」两套口径，避免假绿/假红。
