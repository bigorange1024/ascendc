# 集成契约 — pass-fix-f203-alg13-lines8-15-se-k4 → vec-k4-v3

**拍板（2026-06-22）**：**V3** 为阶段二集成唯一入口；V4 保留为实验路径，**不**接入集成。

---

## 1. 锁定路径（V3）

| 项 | 值 |
|----|-----|
| **变体** | `SE_VECTOR_STAGE=v3`（`run.sh` / CMake 默认） |
| **Kernel** | `f203_se_vector_k4` |
| **Launch** | `blockDim=1`，仅 AIV0 |
| **Phase G** | 标量 Keccak：`DerandFromSeedD` + `HashGSigma` |
| **Phase P** | `shake_xof_kernel`，`batch=8`，`tiling.blockDim=1` |
| **Phase C** | alg8 **P1b-single**（`F203_CBD_BLOCK_DIM=1`） |
| **P→C 同步** | `PipeBarrier<PIPE_ALL>`（SIM 必需） |
| **SIM tick** | 全段 **133153**；分段见 [`SIM_BENCHMARK.md`](SIM_BENCHMARK.md)（G 标量 ~3–8k 估 · P 向量 **83478** · C 向量 **49675**） |

## 2. I/O 契约（与 v2 一致）

| 方向 | 缓冲 | 说明 |
|------|------|------|
| **in** | `seed_d`（uint32） | **唯一** GM 输入 |
| **out** | `src[8,256]` int32 | 行主序；替换 Host `src.bin` |
| **不含** | `sigma.bin` / Host `fips203_build_src` | 集成时删除 |

Golden：SHAKE256 轨；对拍 [`fix-f203-alg13-se-device-scalar-k4`](../frozen/frozen-fix-f203-alg13-se-device-scalar-k4/)。

## 3. 阶段二接线（vec-k4-v3）

```text
Launch 1  f203_se_vector_k4   blockDim=1   SEED_D → src[8,256] GM
Launch 2  vec-k4-v2 内核      blockDim=2   src GM → 行 16–20（NTT / 内积 / ByteEncode）
```

- fork [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) → `vec-k4-v3`
- 删除 Host `input/src.bin` / `fips203_build_src` 注入
- 预采样与 2s1e **分 launch**（单 AIV 预采样 + 双 AIV NTT 段）
- **预验证**：[`CHAIN_NTT17.md`](CHAIN_NTT17.md) — 行 8–17 双 launch CPU/SIM ✅ PASS；SIM **~177553** tick（133153 + 44400）
- 验收：`dst` / `t_hat` / ek / sk 与 v2 一致；SIM tick 记入 `SIM_BENCHMARK.md`

## 4. 刻意不接入

| 项 | 原因 |
|----|------|
| V4（bulk UB / 标量 PRF） | SIM 慢于 V3（158901 / 169768） |
| alg8 P2 双 AIV CBD | NTT 前单 AIV 策略 |
| 本目录内接 NTT | 范围在 vec-k4-v3 |

## 5. 源码入口（集成时引用）

| 文件 | 职责 |
|------|------|
| `f203_se_vector_entry.cpp` | `__global__` 入口 |
| `f203_se_vector.hpp` | G→P→C 编排（`#ifndef F203_SE_VECTOR_V4` 为 V3） |
| `f203_se_vector_prf.hpp` | Phase P |
| `../pass-fix-f203-alg8-cbd-eta2-k4/f203_cbd_eta2*.hpp` | Phase C（P1b-single） |
| `library/shared/shake_xof_kernel/` | PRF 核 |

**CMake 标志**：`-Df203_se_v4=OFF`（默认），`-Dcbd_block_dim=1`。

## 6. 验收命令（集成前回归）

```bash
cd ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4
SE_VECTOR_STAGE=v3 VERIFY_STAGE=all bash run.sh -r cpu -v Ascend910B4
SE_VECTOR_STAGE=v3 bash run.sh -r sim -v Ascend910B4
```

PASS 后本目录可加 `pass-` 前缀（与 [`INTEGRATION.md`](../pass-fix-f203-alg13-lines8-15-se-k4/INTEGRATION.md) 一致；2026-06-28 自 `device-presample-k4` 更名为 `lines8-15-se-k4`）。
