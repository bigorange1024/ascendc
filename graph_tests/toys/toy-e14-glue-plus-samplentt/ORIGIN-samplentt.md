# ORIGIN-samplentt.md — E14 SampleNTT(Â) 接入

## 来源

| 组件 | 路径 |
|------|------|
| **壳** | 复制 `toy-e13-encrypt-shaped-glue/`（未改 E13） |
| **SampleNTT** | 只读参考 `ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg7-sample-ntt-k2/` → `vendor/sample_ntt/` |

## 接入点

| 阶段 | 行为 |
|------|------|
| Host | `input/seed_d.bin` → ws[SD0]；`g.bin` 仍含 v 路 G2 stub |
| L2 AIV 入口 | `F203Alg7::BuildAlg7SampleNttFromSeedD(SEED_D,(0,0))` → ws[G0] |
| | `BuildAlg7SampleNttFromSeedD(SEED_D,(0,1))` → ws[G1] |
| L2 真链 | basemul 读 G0/G1（非 Host stub）；v 路仍读 G2 stub |
| golden | `gen_data.py` 同式 SampleNTT Python oracle + E13 链 |

## 矩阵覆盖（k=2）

| (j,i) | 用途 | 设备 | golden |
|-------|------|------|--------|
| (0,0) | u0 basemul Â | ✅ | ✅ |
| (0,1) | u1 basemul Â | ✅ | ✅ |
| (1,0) | — | ❌ 未做 | ❌ |
| (1,1) | — | ❌ 未做 | ❌ |
| v G2 | stub ĝ_v | Host 加载 | stub |

**语义**：玩具仍按 E13「每 u poly 独立 basemul 单列 Â」简化；非完整 A^T·r 矩阵乘。

## vendor 补丁

`f203_alg7_d12_vec.hpp`：`d1_gm/d2_gm==nullptr` 时跳过 GM 写出（粘合链仅需 â）。

## 未采用

- 抄 Encrypt / Encaps
- 改 E01–E13 / 原探针
- 全 2×2 四 poly SampleNTT（墙钟优先 u 路 2 poly）
