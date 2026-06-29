# frozen-merged-kyber-ntt256 — Phase D

> **已冻结**（2026-06-12）：merged_kyber 7bit 单 poly 基线；golden **非** FIPS `MlkemNtt`。活跃探针：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) — **禁止抄本目录源码** · [frozen/INDEX.md](../INDEX.md)

| | |
|--|--|
| **CPU** | ✓ |
| **SIM** | ✓ |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
