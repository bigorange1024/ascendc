# ⛔ 已冻结（2026-06-15）

**原路径**：`ascendc-tests/fix-f203-alg13-161718-polybatch-sepair-k4-onelaunch/`  
**继任**：[`fix-f203-2s1e-alg13-16171820-k4`](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)（SIM 单 kernel 全路径，无 peer）

## 冻结原因

| 类别 | 说明 |
|------|------|
| **单趟设想难高效实现** | 为去掉 CPU 两段 5→4，引入 `SHAT_PEER` + CrossCore；SIM 上仍依赖 **末路 AIV 串行** 行 18，并未真并行。 |
| **算力利用差** | peer 握手与 workspace 发布/读回占用 MTE；对比 2s1e「每 AIV 本地完整 ŝ」无 AIV↔AIV。 |
| **脆弱集成** | 相对 [sepair-k4](../frozen-fix-f203-alg13-161718-polybatch-sepair-k4/) 仅改 launch/同步壳；根因（se_pair 布局）未消除。 |
| **维护分叉** | 与 sepair-k4 双份维护 `AivAlg13UbPipeline`；2s1e 单一路径验收通过。 |

**勿 fork、勿跑 CI。**
