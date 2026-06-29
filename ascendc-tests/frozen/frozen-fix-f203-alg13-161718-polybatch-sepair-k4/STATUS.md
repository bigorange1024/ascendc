# fix-f203-alg13-161718-polybatch-sepair-k4

> ⛔ **已冻结**（2026-06-15）— 见 [FROZEN.md](FROZEN.md)。继任探针：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)。

Alg.13 行 **16–17–18–19–20**（k=4）：**poly-batch** Tag5T NTT + **se_pair** + 行切分内积 + **AIV 嵌 C ByteEncode₁₂**。

| 项 | 说明 |
|----|------|
| NTT | `AivSplitPolyBatch` / `AivTag5tRouteAModPolyBatch`（同 polybatch-s123） |
| `src`/`dst` | se_pair：`[s0,e0,s1,e1 \| s2,e2,s3,e3]` |
| 行 18 | `AivHatLine18Pair`：AIV0→`t0,t1`；AIV1→`t2,t3`；partial-sum + 对端 ŝ 交换 |
| 行 19–20 | `AivAlg13UbPipeline`：S3→18→encode 单 TPipe UB；仅 ek/sk（及 debug dump）写 GM |
| Golden | NTT：`gen_data.py`；`t_hat`：`hat_inner_product_ref.c`；`ek/sk`：`byte_encode12_ref.c` |

| 阶段 | CPU | SIM |
|------|-----|-----|
| mixPass=0（NTT+行18+encode） | ✓ 两段式（5→4，见 run.sh） | 待测 |
| mixPass=5 NTT only | ✓ | — |
| mixPass=4 行18+19–20 | ✓ | — |
| mixPass=7 encode only | ✓ | — |

```bash
cd ascendc-tests/fix-f203-alg13-161718-polybatch-sepair-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 KERNEL_COMPUTE_BUDGET_SEC=120 bash run.sh -r sim -v Ascend910B4
```

**CPU 说明**：tikicpu 串行 launch AIV，行 18 需对端 `dst` 行已写好；`run.sh` 在 `mixPass=0` 时自动 **5（NTT）→ 4（行18）** 两段。SIM 可单 kernel `mixPass=0`。

上游 NTT 探针（已冻结）：[frozen-fix-f203-tag5t-…-polybatch-s123](../frozen-fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123/)
