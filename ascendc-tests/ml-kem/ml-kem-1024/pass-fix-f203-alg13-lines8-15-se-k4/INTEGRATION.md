# 集成契约 — pass-fix-f203-alg13-lines8-15-se-k4 → vec-k4-v3

**拍板（2026-06-22）**：**V3** 为阶段二集成唯一入口；**v2.5**（原 V4 bulk UB CBD）为实验对照，**更慢、不接入**集成。

---

## 1. 锁定路径（V3，默认）

| 项 | 值 |
|----|-----|
| **变体** | **默认 V3**（`run.sh` / CMake 无需 `SE_VECTOR_STAGE`） |
| **Kernel** | `f203_se_vector_k4` |
| **Launch** | `blockDim=1`，仅 AIV0 |
| **Phase G** | 标量 Keccak：`DerandFromSeedD` + `HashGSigma` |
| **Phase P** | `shake_xof_kernel` UB 单 TPipe，`batch=8`，链末 `DataCopy` → `prf_out` GM |
| **Phase C** | alg8 **P1b-single**（`F203_CBD_BLOCK_DIM=1`）；头文件源 [`pass-fix-f203-alg8-cbd-eta2-k4`](../pass-fix-f203-alg8-cbd-eta2-k4/) |
| **P→C 同步** | `PipeBarrier<PIPE_ALL>`（SIM 必需） |
| **SIM tick** | 全段 **133153**；分段见 [`SIM_BENCHMARK.md`](SIM_BENCHMARK.md)（G 标量 ~3–8k 估 · P 向量 **83478** · C 向量 **49675**） |

### 阶段宏

| 用户 / run.sh | CMake | 设备宏 | 说明 |
|---------------|-------|--------|------|
| （默认）/ `v3` | `F203_SE_V25=OFF` | `F203_SE_VECTOR_V3` | **生产 / 集成** |
| `v2.5` | `F203_SE_V25=ON` | `F203_SE_VECTOR_V25` | bulk UB CBD；SIM ~158k/169k，**禁止集成** |
| `v4`（废弃） | 同 v2.5 + 告警 | 同 v2.5 | 历史名，勿用 |

详见 `f203_se_stage_config.hpp`。

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
| **v2.5**（bulk UB CBD，原 V4） | SIM 慢于 V3（158901 / 169768 vs 133153） |
| alg8 P2 双 AIV CBD | NTT 前单 AIV 策略 |
| 本目录内接 NTT | 范围在 vec-k4-v3 |

## 5. 源码入口（集成时引用）

| 文件 | 职责 |
|------|------|
| `f203_se_stage_config.hpp` | V3 / v2.5 互斥与默认 |
| `f203_se_vector_entry.cpp` | `__global__` 入口 |
| `f203_se_vector.hpp` | G→P→C 编排（`F203_SE_VECTOR_V25` → UB CBD，否则 V3 标量） |
| `f203_se_vector_prf.hpp` | Phase P |
| **Phase C** | V3：`f203_se_vector_cbd_scalar.hpp`；v2.5：`f203_se_vector_cbd_ub.hpp` |
| `library/shared/shake_xof_kernel/` | PRF 核 |

**CMake**：`-DF203_SE_V25=OFF`（默认 V3），`-DF203_CBD_BLOCK_DIM=1`。

## 6. 验收命令（集成前回归）

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines8-15-se-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

目录已于 2026-06-23 加 `pass-` 前缀；2026-06-28 自 `pass-fix-f203-alg13-device-presample-k4` 更名为 `pass-fix-f203-alg13-lines8-15-se-k4`（语义：Alg.13 行 8–15）。
