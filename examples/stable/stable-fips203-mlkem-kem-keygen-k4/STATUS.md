# STATUS — stable-fips203-mlkem-kem-keygen-k4

**语义**：FIPS 203 **Alg.19 `ML-KEM.KeyGen()`**（ml_kem_1024 / k=4）— **stable 定型交付算子**（自 `exp-fips203-mlkem-kem-keygen-k4` 复制晋级，2026-07-14 `#交付#`）。

| 项 | 值 |
|----|-----|
| **状态** | **定型交付**（CPU + SIM + liboqs KAT 已验） |
| **customspec** | [`stable-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf`](stable-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf) |
| **registry** | [`docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md) |
| **Launch** | **2**（prep ‖ mmad+KemKgTailFused） |
| **I/O** | `seed_d`+LUT → `ek_kem` 1568B / `dk_kem` 3168B |
| **SIM tick** | **706633**（stable 交付验收 `SIM_DIRECT=1`，SEED_D=20260619） |

## 踩坑落地（相对 2026-07-13）

| 条款 | 本树实现 |
|------|----------|
| SIM/设备双 AIV 汇合 | Encode 后 `SyncAll<isAIVOnly=true>()`，再 **AIV0** Fuse/Tail |
| CPU 禁 SyncAll 死等 | **AIV1** Fuse/Tail + `dk_kem_gm[0:2]` Encode-done **软旗** |
| 禁空 `KYBER_PIPE_ALL` | `kyber_limb6.hpp` 恒 `PipeBarrier<PIPE_ALL>` |
| 禁残留侥幸 | `run.sh` VERIFY 前清零 `output/*.bin`；host 清零 `sk`/`dk_kem` |

## 验收（晋级门禁 + stable 复验）

| 门禁 | 结果 |
|------|------|
| incubating CPU×40 / SIM / vs correctness×10 | PASS（晋级前） |
| incubating liboqs CPU×10 + SIM×3（家里） | PASS |
| incubating liboqs CPU×10 + SIM×1（2026-07-14 复测） | PASS |
| **stable** `bash run.sh -r cpu` | **PASS** ek/dk max=0（2026-07-14） |
| **stable** `SIM_DIRECT=1 bash run.sh -r sim` | **PASS** tick **706633**；根目录无 stray dump |

```bash
cd examples/stable/stable-fips203-mlkem-kem-keygen-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
KEYGEN_DIR=$PWD KEM_KG_CPU_TRIALS=10 KEM_KG_SIM_TRIALS=1 SIM_DIRECT=1 \
  bash ../../../scripts/liboqs_kem_keygen_batch.sh   # 可选批测
```

**预研副本**：[`exp-fips203-mlkem-kem-keygen-k4`](../../incubating/exp-fips203-mlkem-kem-keygen-k4/)（保留）  
**探针对照**：[`pass-fix-f203-alg19-kem-keygen-device-k4`](../../../ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/)
