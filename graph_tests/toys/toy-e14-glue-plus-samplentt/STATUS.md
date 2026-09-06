# STATUS.md — toy-e14-glue-plus-samplentt

## 目标

E13 Encrypt 粘合 + L2 **真 SampleNTT(Â)** 替换 u 路 stub；≥3 轮 SIM 不挂；尽量 golden。

## 实现摘要

| 项 | 状态 |
|----|------|
| 壳 | 自 `toy-e13-encrypt-shaped-glue/` 复制 |
| SampleNTT vendor | `vendor/sample_ntt/`（Alg.7 k=2 探针自包含） |
| u 路 Â | 设备 `BuildAlg7SampleNttFromSeedD(SEED_D,(0,0)/(0,1))` → ws[G0/G1] |
| v 路 | G2 Host stub（同 E13） |
| L1 / c1∥c2 / SET(4) | 保留 E13 真链 |
| golden | `gen_data.py` Python SampleNTT oracle + 384B c |

## 矩阵覆盖（诚实）

- **已做**：u 路 2/4 元 `(0,0)`、`(0,1)` — 满足「≥2 真 SampleNTT poly 参与 u 路」
- **未做**：`(1,0)`、`(1,1)` 与 v 路 SampleNTT（墙钟优先）

## 同步

- AIV subBlock0：SampleNTT → GM `SNTT_FLAG=1` → CrossCore SET(7) 唤醒 AIC
- AIV subBlock1：GM 轮询 `SNTT_FLAG`（避免与 AIC 抢 CrossCore7）
- AIC：`SampleNttWait(7)` 后进 NTT 环

## 验收命令

```bash
cd graph_tests/toys/toy-e14-glue-plus-samplentt
bash run.sh -r cpu -v Ascend910B4          # CPU 慢（SampleNTT×2/轮）；可 TOY_ROUNDS=1
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 验收结果

| 模式 | 结果 | 备注 |
|------|------|------|
| CPU | **未跑满** | SampleNTT×2/轮 CPU 极慢；SUBAGENT SIM-only |
| SIM ×3 | **PASS** | golden 0/384B；~420s kernel；402×3 |

验收命令（已通过 SIM）：

```bash
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
