# frozen-fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123

> **已冻结**（2026-06-12）：迁入 `ascendc-tests/frozen/`；limb 面对半 Stage3 历史对照，**勿 fork**。  
> 继任活跃探针：[fix-f203-2s1e-alg13-16171820-k4](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) · [frozen/INDEX.md](../INDEX.md) — **禁止抄本目录源码**

FIPS 203 正向 NTT（Tag5T 转置乘）：**Stage1+2+3** 全链路。

| 张量 | 形状 | 说明 |
|------|------|------|
| `lut_stacked.bin` | `[512,256]` int8 | 上 256 行 = `L^T` 左半列，下 256 行 = 右半列 |
| `src` | `[8,256]` int32 | 8×同 poly |
| `S0` | `[16,256]` int8 | Tag5T `R^T`，`[HI_8\|LO_8]` |
| `mat_c` | `[32,256]` int32 | 上 16 行 = `C_lo`，下 16 行 = `C_hi` |
| `dst` | `[8,256]` int32 | RouteA 合并 + mod q（golden = `MlkemNtt`） |

Stage3 用 `AivTag5tRouteAMod`：Gather 解交织 + 合并/mod（**三种取模可切换**，见 `STAGE3_VARIANTS.md`、`stage3_config.hpp` 之 `F203_STAGE3_MOD`，默认 **2=Cast+Div**）。

## 调试

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
TAG5T_MIX_PASS=3 bash run.sh -r cpu   # 仅 Stage3
```

超时：CPU=15s，SIM=20s（全链路 SIM ~10s）。

## 状态

| 阶段 | CPU | SIM |
|------|-----|-----|
| Stage1 Split | ✓ | ✓ |
| Stage2 2×`AicMmad(16,256,256)` | ✓ | ✓ |
| Stage3 RouteA+Cast/Div mod | ✓ | ✓ ~10s |
