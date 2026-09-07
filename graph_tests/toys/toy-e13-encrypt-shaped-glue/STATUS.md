# STATUS — toy-e13-encrypt-shaped-glue

| 项 | 值 |
|----|-----|
| task | E13 / D-exp-e13 |
| verdict | **PASS** |
| k | 2（u 路两 poly；v 路单 poly） |
| launch | 2-launch（L1 采样 / L2 代数+压码）+ SET(4) |
| 输出 | **c=384B** = c1(256B)∥c2(128B) |
| μ | 仅 v 路 Decompress_1(μ) |

## c 形布局（冻结）

| 段 | 偏移 | 长度 | 语义 |
|----|------|------|------|
| **c1** | 0 | 256B | u0∥u1 ByteEncode_d4（INTT 后 **无** μ） |
| **c2** | 256 | 128B | v ByteEncode_d4（INTT 后 **含** Decompress_1(μ)） |

## 验收（2026-09-06）

```bash
cd graph_tests/toys/toy-e13-encrypt-shaped-glue
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | 结果 | kernel wall |
|------|------|-------------|
| CPU ×3 | PASS | ~12s |
| SIM ×3 | PASS | ~253s（budget 1500） |

Golden：SHAKE 0/32；CBD(u) 0/512；**c 0/384B**。

## 几何摘要

- `src`：512 int32（u 路 r0∥r1）
- `ws[E0]`：256 int32（v 路 e2，L1 CBD）
- `prf`：384B（3×128B）
- `g.bin`：768 int32（ĝ_u0∥ĝ_u1∥ĝ_v stub）
- A/Minv：固定 toy 矩阵（stub 公钥，见 ORIGIN-glue.md）

## 日志

- `/opt/cursor/artifacts/e13-cpu.log`
- `/opt/cursor/artifacts/e13-default-sim.log`
- `output/host_trace.log`
