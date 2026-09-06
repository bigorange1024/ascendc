# STATUS.md — toy-e15-samplentt-a-full-2x2

## 目标

E14 壳 + **完整 2×2 Â 真 SampleNTT**；≥3 轮 SIM 不挂；尽量 golden。

## 实现摘要

| 项 | 状态 |
|----|------|
| 壳 | 自 `toy-e14-glue-plus-samplentt/` 复制 |
| SampleNTT vendor | `vendor/sample_ntt/`（Alg.7 k=2 自包含） |
| Â 2×2 | 设备 `BuildAlg7SampleNttFromSeedD(SEED_D,(0,0)…(1,1))` → ws[G0…G3] |
| v 路 | G2=(1,0) 真 SampleNTT（非 Host stub） |
| L1 / c1∥c2 / SET(4) | 保留 E14 真链 |
| golden | `gen_data.py` 四元 SampleNTT oracle + 384B c |

## 矩阵覆盖

| (j,i) | ws | L2 消费 | golden |
|-------|-----|---------|--------|
| (0,0) | G0 | u0 basemul | ✅ |
| (0,1) | G1 | u1 basemul | ✅ |
| (1,0) | G2 | v basemul | ✅ |
| (1,1) | G3 | 采样落盘 | ✅ |

## 同步

- 独立 launch phase=1（禁止嵌 MIX 真链同 kernel）
- AIV subBlock0：4×SampleNTT 串行 → G0…G3
- L2：AIC `SampleNttWait(7)` 后进 NTT 环（同 E14）

## 验收命令

```bash
cd graph_tests/toys/toy-e15-samplentt-a-full-2x2
bash run.sh -r cpu -v Ascend910B4          # CPU 极慢；可 TOY_ROUNDS=1
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 验收结果

| 模式 | 结果 | 备注 |
|------|------|------|
| CPU ×1 | **PASS** | `TOY_ROUNDS=1`；golden 0/384B |
| SIM ×3 | **PASS** | golden 0/384B；kernel ~429s；300/302/303/304/305×3 |

验收命令（已通过）：

```bash
TOY_ROUNDS=1 bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
