# ORIGIN-k2.md — E12 k=2 几何与输入策略

## 来源

- **壳**：复制 `toy-e11-chain-plus-decompress-mu/`（未改 E11）
- **k=2 语义**：对齐 ML-KEM-512 模块秩 k=2 的「两 poly 独立链」最小形态

## 两路 poly 策略

| 项 | 约定 |
|----|------|
| CBD | `prf.bin` 256B = row0∥row1 各 128B；L1 `SamplePolyCbd2OneRowUb(row=0,1)` |
| NTT/basemul/INTT | L2 **串行** poly0→poly1；CrossCore flag 复用，poly 间 `PipeBarrier` |
| ĝ | `g.bin` 512 int32：ĝ₀=(13·i+7)%q，ĝ₁=(13·i+7+997)%q |
| μ | **共享** 32B（`SEED_D+1`）；两 poly 均做 Decompress_1 嵌入（toy 简化，非 Alg.14 最终 v-only 语义） |
| 输出 | poly0 ByteEncode→out[0:128]；poly1→out[128:256] |

## 与 E11 差异

- `tiling.h`：`k=2`，`W0/W1` 工作 poly，`G0/G1`，尺寸翻倍
- L1：`RunCbdEta2K2Polys` + TRACE 222
- L2：`L2RealChainOnePolyAiv` ×2；AIC `L2RealNttBasemulInttAicOnePoly` ×2；**一次** SET(4)
- golden：`gen_data.py` 逐 poly 链式计算后拼接 256B

## 未采用

- 并行两 poly 占双 AIV（避免 CrossCore 与 UB 几何爆炸）
- 省略 Decompress/Compress（时间允许，全链保留）
- 抄 Encrypt / polyvec4 整图
