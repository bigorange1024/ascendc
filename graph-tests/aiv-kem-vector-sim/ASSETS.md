# 资产清单 · aiv-kem-vector-sim（单 AIV · 全 AscendC）

> 刷新：2026-09-12  
> 口径：ML-KEM-1024；SIM cpu + `SIM_DIRECT=1`；权威 liboqs；Host 仅 `ek|m(|coins)` + NTT 常量表 + workspace GM

## 1. 架构收益（沉淀）

| 点 | 含义 |
|----|------|
| 单 AIV 闭环 | 单用例单 launch，无 AIC↔AIV / CrossCore / GATE 组件同步 |
| 单 AIV 可串多例 | 同一核按序跑多个 `(ek,m)`，同步仍在核内 |
| 多 AIV = 多任务 | 有多少 AIV 即可并行多少用例，任务间无 Sync |
| 与 cann-ntt 正交 | cann-ntt = MIX+Cube 多段；本线 = AIV-only 全设备密码学 |

## 2. 代码树（可提交源）

### 2.1 战役壳

| 路径 | 角色 |
|------|------|
| `INDEX.md` | 战役入口 / 状态 |
| `PLAN.md` / `QUEUE.md` | 波次与门禁 |
| `ASSETS.md` | 本清单 |
| `tasks/TASK-AE-FULL.md` | 初版 Encrypt/Encaps 任务书 |
| `tasks/TASK-AE-FULL-ASCENDC.md` | 全 AscendC（消 Host Â/t̂）任务书 |

### 2.2 Encrypt · `AE-E-encrypt/`

| 路径 | 角色 |
|------|------|
| `aiv_encrypt.cpp` | 单 AIV：CBD + SampleNTT×16 + ByteDecode₁₂ + NTT/matvec/INTT/pack |
| `main.cpp` | Host：读 `ek\|m\|coins`+表，workspace 仅 GmAlloc |
| `scripts/gen_data.py` / `verify_result.py` | liboqs fixture + 对拍 |
| `run.sh` / `CMakeLists.txt` / `cmake/*` | cpu/sim 壳 |
| `STATUS.md` | 验收；SIM tick≈**1885458**；c max=0 |

### 2.3 Encaps · `AE-P-encaps/`

| 路径 | 角色 |
|------|------|
| `aiv_encaps.cpp` | 单 AIV：FO + CBD + SampleNTT + ByteDecode + Encrypt |
| `main.cpp` | Host：读 `ek\|m`+表 |
| `scripts/*` / `run.sh` / CMake | 同上 |
| `STATUS.md` | SIM tick≈**1977711**；c/K max=0 |

### 2.4 不入库

`build/` `out/` `input/` `output/` `sim_log/` `generated/` `*.bin` `OPPROF_*` stray dump

## 3. 共享积木（本线依赖）

| 路径 | 用途 |
|------|------|
| `library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp` | SHA3/SHAKE256；本线补 **Shake128OneShot** |
| `library/shared/f203_byte_codec/byte_decode12_vec.hpp` | ByteDecode₁₂ |
| `graph-tests/aiv_ntt/AV01-*` | 正向 NTT 积木基线（Gather 蝶形；SIM≈7871） |

## 4. 文档 / 图谱 / 纪要

| 路径 | 用途 |
|------|------|
| `docs/notes/Encrypt-aiv-vector-kb.md` | 本线知识库 |
| `docs/notes/Encrypt-aiv-vector-capability-inventory.md` | 能力清单 |
| `docs/rg-encrypt-aiv-vector.yaml` | 推理图谱 |
| `qa/2026-09/2026-09-12-单AIV全向量-Encrypt-Encaps-SIM.md` | 当日纪要 |
| `qa/2026-09/2026-09-12-单AIV-MLKEM-NTT抽取与SIM对比.md` | AV01 对照 |

## 5. 性能锚点（SIM Total tick）

| 实现 | Encrypt | Encaps | 说明 |
|------|---------|--------|------|
| **本线 DEVICE_FULL** | ≈1.89M | ≈1.98M | 全设备；输入纯 ek\|m(\|coins) |
| 本线 HOST_SAMPLE 过渡 | ≈1.20M | ≈1.38M | Â/t̂ 曾 Host |
| cann-ntt EN13/EP04 | ≈0.81M | ≈0.81M | MIX+Cube；部分 Prep 仍 Host |

SIM tick ≠ 真机 µs；NPU 未测。

## 6. 复验命令

```bash
cd graph-tests/aiv-kem-vector-sim/AE-E-encrypt
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

cd ../AE-P-encaps
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
