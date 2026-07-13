# STATUS — pass-merged-kyber-mix-ntt256

| 项 | 内容 |
|----|------|
| **角色** | 本仓 **授权示例用例**：Kyber 风格 **单 poly n=256** MIX NTT（非 FIPS Tag5T 交付路径） |
| **来源** | 原 `thirdparty/merged_kyber`（upstream [MmadBiasInvocation1](https://github.com/serial2007/MmadBiasInvocation1)，作者已授权迁入本工程） |
| **阶段** | **CPU+SIM PASS**（2026-07-13 迁入复验）；SIM tick **10348** |
| **自包含** | 本目录完整可编可跑；**不再**依赖 `thirdparty/merged_kyber` |

## 算什么

设备侧 FSM：`AIV Split` → `AIC Mmad`×2 → `AIV Merge` + Barrett。  
Host golden：`scripts/ntt_sim_kyber.py`（与设备 **I/O 对拍**；不作 AscendC 实现规格）。

**注意**：本用例 golden 为 merged_kyber / `ntt_sim_kyber` 语义，**不等于** FIPS `MlkemNtt` / Tag5T RouteA。F203 交付 NTT 见 `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`。

## 与 frozen 的关系

历史上基于本示例 fork 的 poly2/poly8 / limb6 等探针已在 `ascendc-tests/frozen/frozen-*-merged-kyber-*` **路线关闭**。本目录是 **上游示例本体的工程内落点**，不是把 frozen 解冻。

## 验收

```bash
cd ascendc-tests/pass-merged-kyber-mix-ntt256
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | 结果 |
|------|------|
| CPU | **PASS**（error ratio 0） |
| SIM | **PASS**（tick **10348**；需 `sim_env.sh` 防 WSL FPE） |
