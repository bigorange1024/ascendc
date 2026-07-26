# pass-fix-f203-alg13-lines8-15-se-k4 — STATUS

**Alg.13 行 8–15**：`SEED_D` → Phase G+P+C → `src[8,256]`。

## 阶段宏（默认 V3，无需手动 export）

| 标识 | CMake | 编译宏 | 默认 | 集成 |
|------|-------|--------|------|------|
| **V3 生产** | `-DF203_SE_V25=OFF` | `F203_SE_VECTOR_V3=1` | **是** | KeyGen / vec-k4-v3 |
| **v2.5 实验** | `-DF203_SE_V25=ON` | `F203_SE_VECTOR_V25=1` | 否 | **禁止**（SIM 更慢） |
| ~~v4~~ | — | 废除；别名 → V25 | — | — |

集中说明：`f203_se_stage_config.hpp`。

## 验收

| 模式 | 阶段 | 结果 | 证据 |
|------|------|------|------|
| CPU | V3（默认） | ✅ | `stage=V3-production`；golden 对拍 max_abs_diff=0 |
| SIM | V3（默认） | ✅ | 无 `pem_lsu` 告警；golden 对拍 PASS；tick **95261**（待复录分段表） |

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines8-15-se-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

调试 v2.5（非默认）：`SE_VECTOR_STAGE=v2.5 bash run.sh -r sim -v Ascend910B4`

## 回归备忘（2026-06-25）

**SIM `pem_lsu` 告警根因**：Phase P 迁移到 UB SHAKE 后仍用 `maxMsgLen=33` 作行 stride → `msgBase=33,66,99…` 非 8B 对齐，`shake_general.h` 的 `XorBlock32` 触发 LSU 告警且 PRF 从 row3 起损坏。

**修复**：`PRF_MSG_STRIDE=64`（`lengths[i]` 仍为 33）；`main.cpp` tiling `kMaxMsgLen=64` 与内核一致。V3 Phase C 仍用 alg8 `SamplePolyCbd2Batch8`（非 `cbd_scalar`）。

**与历史 tick**：文档记载全段 **133153** 为旧 `ccec` 构建 + 当时源码；当前 ascendc_library 路径 SIM **95261**，功能对拍 PASS，分段 tick 待复录。

- 集成契约：[`INTEGRATION.md`](INTEGRATION.md)
- 链式 8–17：[`CHAIN_NTT17.md`](CHAIN_NTT17.md)
- KeyGen G2：[`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/)
