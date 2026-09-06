# ORIGIN-a-2x2.md — E15 完整 2×2 Â SampleNTT

## 来源

| 组件 | 路径 |
|------|------|
| **壳** | 复制 `toy-e14-glue-plus-samplentt/`（未改 E14） |
| **SampleNTT** | 复用 E14 `vendor/sample_ntt/` |

## 相对 E14 增量

| 项 | E14 | E15 |
|----|-----|-----|
| SampleNTT 元 | (0,0)(0,1) | **+ (1,0)(1,1)** |
| G 区 | G0/G1 设备；G2 Host stub | **G0…G3 全设备** |
| v basemul | G2 stub | **G2=(1,0) 真 â** |
| tiling | kGBytes=(k+1) poly | **k×k=4 poly** |
| TRACE | 300/302/303 | **+304/305** |

## 接入点

| 阶段 | 行为 |
|------|------|
| Host | `seed_d.bin` → ws[SD0]；**不**预载 g.bin |
| phase=1 | subBlock0：`BuildAlg7SampleNttFromSeedD` ×4 → G0…G3 |
| phase=2 L2 | u0/u1 读 G0/G1；v 读 G2；G3 完整性 |
| golden | 四元 Python SampleNTT + E13 链 |

## 硬约束（继承 E14）

- SampleNTT **必须独立 launch/phase**；禁止嵌进 MIX 真链同 kernel（TPipe 互抢 SIM 挂）
- 未抄 Encrypt / 未改 E01–E14

## 语义说明

玩具仍按 E13「每 poly 独立 basemul 单列 Â」简化；**非**完整 A^T·r 矩阵乘。完整 2×2 指 **四元均设备采样**，非四路 basemul 累加。
