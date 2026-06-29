# fix-f203-tag5t-ntt256-limb6-poly8-planar-s12

> ⛔ **已冻结**（2026-06-15）— 见 [FROZEN.md](FROZEN.md)。平面 S1+2 已并入 [fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)。
**目标**：Stage2 输出 **平面 mat_c** `[64,128]`（每 poly 四行 `hh|lh|hl|ll`，无列交错），为后续无 Gather Stage3 做准备。

| 阶段 | 实现 |
|------|------|
| S1 | `AivSplitPolyBatch`（整块 `DataCopy`） |
| S2 Cube | `4×AicMmad(16,256,128)` — 偶/奇 LUT 列分乘 |
| S2 AIV | `AivPackMatCPlanar` — 行重排至 `mat_c_planar`（仅 `DataCopy`） |

**mat_c 行序**（poly `p`，half `0=C_lo` / `1=C_hi`）：

```text
row = half*32 + p*4 + limb   # limb: 0=hh, 1=lh, 2=hl, 3=ll
```

| mixPass | 含义 |
|---------|------|
| 0 | S1+S2 |
| 1 | 仅 S1 |
| 2 | 仅 S2（需 `s0_preset.bin`） |

| 模式 | 状态 |
|------|------|
| CPU | ✓ |
| SIM | ✓ |

**对照**：golden 与权威交错 `mat_c[32,256]` 经列 `0::2/1::2` 推导一致（`gen_data.py` cross_check）。
