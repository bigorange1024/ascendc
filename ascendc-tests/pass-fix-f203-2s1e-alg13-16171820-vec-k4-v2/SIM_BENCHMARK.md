# vec-k4-v2 — SIM 性能基准（Ascend910B4）

**复测日期**：2026-06-19（ByteEncode prefetch 合入后）  
**命令**：各目录 `bash run.sh -r sim -v Ascend910B4`；tick 取自 camodel `[INFO] Total tick`。

---

## 1. 合入 v2 的三块单用例（独立探针）

| 模块 | 独立探针 | 默认配置 | SIM Total tick | 对拍 |
|------|----------|----------|----------------|------|
| **行 16–17 NTT**（S1+S2+S3） | 无单独活跃目录；以 **v2 `mixPass=5`** 定标 | `F203_STAGE1_SPLIT=1` | **44648** | dst / ŝ / ê_hat（不含 t_hat） |
| **行 18 内积（全量单 AIV）** | [`innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/) | 4×4×1，`a_hat` 行主序 | **43992** | t_hat max=0 |
| **行 18 内积（半行双 AIV）** | [`innerproduct-k4-halfrows`](../pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) | 双 AIV 半行 | **26185** | t_hat max=0 |
| **行 19–20 ByteEncode** | [`byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/) | `PREFETCH=1` / `PREFETCH=0` | **17429** / **25464** | ek/sk max=0 |

说明：

- NTT 代码内嵌于 v2（源自 Tag5T polybatch S123 平面 mat_c 路线）；**勿**用 frozen polybatch 目录 tick 对照。
- 内积单用例 **43992**（全量）/ **26185**（halfrows）低于融合 dot-only 边际 **+20840**，因融合含 NTT 后 UB 布局、Alg11 ROM、单 TPipe 等额外开销（见 [F203-2s1e-NTT内积UB融合技术总结.md](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md) §4.1）。
- ByteEncode 单用例为 **encode-only**（preset GM）；全链路边际 **+12421**（prefetch）因 t̂/ŝ 已在 UB。

---

## 2. v2 融合分段（`mixPass=0` 或显式 mixPass）

| 配置 | 命令要点 | SIM Total tick | 对拍 |
|------|----------|----------------|------|
| 仅 NTT S1–S3 | `NTTS2S1E_MIX_PASS=5` | **44648** | dst；t_hat/ek/sk 不测 |
| NTT + 行18 dot | `HAT_LINE18_DOT_ONLY=1 HAT_BYTE_ENCODE=0` | **65488** | `golden_t_hat_dot` |
| +ê 无 encode | `HAT_BYTE_ENCODE=0` | **65537** | `golden_t_hat` |
| **全链路 prefetch** | 默认 `bash run.sh` | **77958** | dst / t_hat / ek / sk |
| 全链路 tile32 encode | `BYTE_ENCODE12_PREFETCH=0` | **85991** | 同上 |

### 2.1 边际分解（prefetch 默认）

| 段 | tick | 相对上一段 |
|----|------|------------|
| NTT（mixPass=5） | 44648 | — |
| +行18 内积 | 65488 | **+20840** |
| +ê | 65537 | **+49** |
| +ByteEncode prefetch | 77958 | **+12421** |
| （对照）+ByteEncode tile32 | 85991 | **+20454** |

相对 6/18 tile32 全链路 **86120**：prefetch **−8162 tick（−9.5%）**。

---

## 3. 复现

```bash
# 单用例
cd ascendc-tests/pass-fix-f203-alg11-12-innerproduct-k4-halfrows && bash run.sh -r sim -v Ascend910B4
cd ascendc-tests/pass-fix-f203-2s1e-byteencode12-vec-k4 && bash run.sh -r sim -v Ascend910B4

# v2 分段
cd ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2
NTTS2S1E_MIX_PASS=5 bash run.sh -r sim -v Ascend910B4
HAT_LINE18_DOT_ONLY=1 HAT_BYTE_ENCODE=0 bash run.sh -r sim -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
BYTE_ENCODE12_PREFETCH=0 bash run.sh -r sim -v Ascend910B4
```

---

*与 [STATUS.md](STATUS.md)、[qa/2026-06-19 纪要](../../qa/2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md) §8–§9、[exp-k4 STATUS](../../examples/incubating/exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4/STATUS.md) 同步。*
