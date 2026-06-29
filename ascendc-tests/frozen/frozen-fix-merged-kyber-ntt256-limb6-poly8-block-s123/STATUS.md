# frozen-fix-merged-kyber-ntt256-limb6-poly8-block-s123

> **⛔ 路线否决**（2026-06-19 补强；2026-06-12 归档）：块紧凑 S0 `[HI_8|LO_8]` **不适合** Atlas A2 活跃 NTT 工程。详见 [FROZEN.md](FROZEN.md)。**禁止抄码、禁止参考、禁止 fork。**

8×同 poly 全链路：batch Split（**块紧凑 S0 `[HI_8|LO_8]`**）+ 2×Mmad(16,256,256) + batch Merge（Gather 行 `p` / `8+p`）。

与 [`frozen-fix-merged-kyber-ntt256-limb6-poly8-s123`](../frozen-fix-merged-kyber-ntt256-limb6-poly8-s123/) 差异：仅 Stage1 写址与 Stage3 读址；**M4.bin / Stage2 不变**。SIM 性能与交错版无显著差异；在 ntt_study（1C:1AIV）下 block/交错亦无差别，**不可外推至本仓 A2**。

| 张量 | 形状 |
|------|------|
| src | `[8,256]` int32 |
| S0 | `[16,256]` int8，行序 `[hi0..hi7 \| lo0..lo7]` |
| dst / golden | `[8,256]` int32（NTT，与 poly8-s123 一致） |

**继任**：交错探针 [`poly8-s123`](../frozen-fix-merged-kyber-ntt256-limb6-poly8-s123/)（历史）；活跃 [`exp-sepolyvec8-ntt-k8`](../../../examples/incubating/exp-sepolyvec8-ntt-k8/)；全链路 [`vec-k4-v2`](../../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)。

| 模式 | 状态 |
|------|------|
| CPU  | ✓ `max_abs_diff=0`；dst ≡ poly8-s123 golden |
| SIM  | ✓ `SIM_DIRECT=1`；`max_abs_diff=0` |

```bash
# 仅历史复现；勿 fork 到活跃探针
bash run.sh -r cpu -v Ascend910B4
```
