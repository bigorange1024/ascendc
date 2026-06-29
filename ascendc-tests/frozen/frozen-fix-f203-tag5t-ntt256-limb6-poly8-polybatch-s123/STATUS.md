# fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123

> ⛔ **已冻结**（2026-06-15）— 见 [FROZEN.md](FROZEN.md)。继任集成探针：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)。

**历史角色**（2026-06-12）：Tag5T poly-batch NTT-only 权威探针。

**架构约束**：Stage2 后每个 AIV 握有完整 poly 的 hi+lo；禁止单 poly 高低位分属不同 AIV。详见 [MLKEM-NTT-实现总结](../../docs/notes/MLKEM-NTT-实现总结.md)。

| 项 | 说明 |
|----|------|
| S0 行序 | `[hi0..hi3, lo0..lo3, hi4..hi7, lo4..lo7]` |
| Stage2 后 | AIV0: C_lo[0:8)+C_hi[16:24)；AIV1: C_lo[8:16)+C_hi[24:32) |
| Stage3 | `AivTag5tRouteAModPolyBatch`：两 AIV 同代码、无互相同步 |
| 输出 | `dst [8,256]` int32 |

| 阶段 | CPU | SIM |
|------|-----|-----|
| mixPass=0 全链路 | ✓ | ✓ |
| mixPass=3 仅 S3 | — | — |

```bash
cd ascendc-tests/fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

数值对照（勿作新实现基线）：[fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123](../frozen/frozen-fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123/)（limb 面对半 Stage3，同输入 poly 下 dst 一致）
