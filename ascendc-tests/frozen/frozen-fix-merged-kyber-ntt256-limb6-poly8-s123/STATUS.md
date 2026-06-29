# frozen-fix-merged-kyber-ntt256-limb6-poly8-s123

> **已冻结**（2026-06-12）：golden=`ntt_sim_kyber`，**非** FIPS `MlkemNtt`。F203 活跃探针：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) — **禁止抄本目录源码**。

8×同 poly 全链路：batch Split + 2×Mmad(16,256,256) + batch Merge（单 TPipe）。

| 张量 | 形状 |
|------|------|
| src | `[8,256]` int32 |
| dst / golden | `[8,256]` int32（NTT） |

每行 NTT 与 [`frozen-merged-kyber-ntt256-limb6`](../frozen-merged-kyber-ntt256-limb6/) 单 poly golden 一致。

| 模式 | 状态 |
|------|------|
| CPU  | ✓ `max_abs_diff=0`；8 行均 ≡ limb6 golden |

```bash
bash run.sh -r cpu -v Ascend910B4
```
