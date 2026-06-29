# fix-f203-alg13-161718-polybatch-sepair-k4-onelaunch

> ⛔ **已冻结**（2026-06-15）— 见 [FROZEN.md](FROZEN.md)。继任：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)。

基于 [frozen-fix-f203-alg13-…-sepair-k4](../frozen-fix-f203-alg13-161718-polybatch-sepair-k4/)：**单趟** `mixPass=0`（一次 PEM/SIM launch）完成 NTT→行18→ByteEncode。

| 项 | 说明 |
|----|------|
| ŝ 对端握手 | `ws+SHAT_PEER` 发布整片 `kPolysPerAiv` ŝ‖ê tile；SIM 串行下 **末路 AIV** 统一行18+encode |
| 与 k4 差异 | 无 `run.sh` 5→4 两段；无 GM `volatile` 自旋；无 dst 算法往返 |
| UB | `AivAlg13UbPipeline` 单 TPipe；仅 ek/sk（及 debug dump）写 GM |

| 阶段 | CPU | SIM |
|------|-----|-----|
| mixPass=0 全量单趟 | ✓ | ✓ |
| mixPass=5 NTT only | ✓ | — |
| mixPass=4 行18+encode（preset） | ✓ | ✓ |
| mixPass=7 encode only | ✓ | ✓ |

```bash
cd ascendc-tests/fix-f203-alg13-161718-polybatch-sepair-k4-onelaunch
bash run.sh -r cpu -v Ascend910B4
KERNEL_COMPUTE_BUDGET_SEC=120 bash run.sh -r sim -v Ascend910B4
```

上游（已冻结）：[frozen-fix-f203-alg13-…-sepair-k4](../frozen-fix-f203-alg13-161718-polybatch-sepair-k4/)
