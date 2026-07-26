# pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2 — 行 16–20 向量集成方案

**更新**：2026-05-24（行 21 `ek_PKE = ek_polyvec ‖ ρ` 内核融合；`F203_KEYGEN_EK_PKE=1` 默认开启）

**文档索引**（勿在本目录写长篇经验教训）：

| 类型 | 路径 |
|------|------|
| 技术总结 / 踩坑 / 加解密参考 | [docs/notes/F203-2s1e-NTT内积UB融合技术总结.md](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md) |
| ByteEncode prefetch | [docs/notes/F203-ByteEncode12-prefetch技术总结.md](../../docs/notes/F203-ByteEncode12-prefetch技术总结.md) |
| 讨论与决策 | [qa/2026-06/2026-06-18-…](../../qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md)、[qa/2026-06-19 ByteEncode](../../qa/2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md) |
| SIM 性能 | [SIM_BENCHMARK.md](SIM_BENCHMARK.md) |
| 验收状态 | [STATUS.md](STATUS.md) |

## 当前状态（2026-06-19）

| 项 | 状态 |
|----|------|
| `HAT_LINE18_DOT_ONLY=1`（Â·ŝ dot） | CPU/SIM PASS；SIM **65488 tick** |
| `HAT_LINE18_DOT_ONLY=0` `HAT_BYTE_ENCODE=0`（+ê） | PASS；SIM **65537 tick** |
| **全链路** `BYTE_ENCODE=1` `PREFETCH=1` | CPU/SIM PASS；SIM **77958 tick**（<100k） |
| **行 21** `F203_KEYGEN_EK_PKE=1` | mixPass=4 单 launch：`ek_pke` CPU/SIM **PASS**（47684 tick）；无额外 launch |
| 全链路 tile32 encode（`PREFETCH=0`） | SIM **85991 tick** |
| 行 18 | **j→p** 全 poly `compute_on_ub`；`HAT_LINE18_FULLPOLY=1` |
| ŝ 驻留 | S3→行18 计算路径内不离 UB |
| ByteEncode | [`byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/) prefetch **已合入** |

## 相对已冻结 v1

| 项 | v1（`frozen-…-vec-k4`） | v2（本目录） |
|----|-------------------------|--------------|
| 行 18 循环 | half→p→j | **j→p** 外循环 |
| basemul | `multiply_ntts_half_vec` | `compute_on_ub`（内积同源） |
| TPipe | 大 `scratch_` hat 工作区 | slim TPipe + `dotScratchBuf_` |
| 全链路 SIM tick | ~137k | **77958**（prefetch）；85991（tile32 encode） |

v1 冻结说明：[frozen-fix-f203-2s1e-alg13-16171820-vec-k4/FROZEN.md](../frozen/frozen-fix-f203-2s1e-alg13-16171820-vec-k4/FROZEN.md)

## 内积单用例与布局（2026-06-18）

融合 `stageHatDotOnly` 与单用例探针必须使用 **同一 `a_hat` 读法** `(p*K+j)*N`：

- 全量探针：[`innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/) — SIM **43992**，PASS
- 半行探针：[`innerproduct-k4-halfrows`](../pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) — SIM **26185**，PASS（2026-06-19 复测）

纪要：[qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md](../../qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md) §1

---

## 目的

在 [`fix-f203-2s1e-alg13-16171820-k4`](../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) 全链路上，合并已验收向量 fork：

| 上游探针 | 替换段 | 默认宏 |
|----------|--------|--------|
| [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/) | 行 **19–20** `ByteEncode₁₂` | `BYTE_ENCODE12_VEC=1` **`BYTE_ENCODE12_PREFETCH=1`**（2026-06-19 合入） |
| [`pass-fix-f203-alg11-12-innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/) | 行 **18** j→p 内积 / basemul | `ALG11_IMPL=1` `ALG11_VEC_VARIANT=2` `ALG11_MEM_OPS=1` |

行 **16–17**（Stage1–3 NTT）保持与基线相同。

## 数据流

```text
host 1s+1e + â[16,256] + ρ[32]
  → Stage1–3 NTT（行 16–17）
  → Aiv2s1eUbPipeline（单 TPipe）
       stageS3Into → ŝ/ê_hat UB
       stageHatDotOnly → t̂（行 18：Σ_j + ê，mod Q）
       stageEncodeOut → ek/sk（行 19–20）
  → FuseEkPke（行 21，AIV0 同 launch 末尾）→ ek_PKE = ek_polyvec ‖ ρ
```

**CPU/SIM mixPass=0**：仍为 **5→4 两段 launch**（S2 MMAD 孪生不可靠）；**ρ 拼接仅在第二段 pass4**，与行 18–20 同 kernel，**禁止**第三次 launch 或 Host `ek_append`。

## 宏（CMake / run.sh）

| 宏 | 全链路默认 | 含义 |
|----|------------|------|
| `HAT_LINE18_DOT_ONLY` | `0` | `1`=仅 Â·ŝ；`0`=+ê |
| `HAT_BYTE_ENCODE` | `1` | `1`=行 19–20 encode |
| `HAT_LINE18_FULLPOLY` | `1` | j→p `compute_on_ub` 路径 |
| `HAT_ALG11_VEC` | `1` | Alg11 向量核 |
| `BYTE_ENCODE12_VEC` | `1` | 向量 encode |
| `BYTE_ENCODE12_PREFETCH` | `1` | 整 poly ROM+Gather（`0`=tile32 legacy） |
| `F203_KEYGEN_EK_PKE` | `1` | `1`=行 21 内核融合 `ek‖ρ`；`0`=仅输出 `ek_polyvec`（调试） |

## 行 21 实现要点

| 项 | 说明 |
|----|------|
| 宏 | `F203_KEYGEN_EK_PKE=1`（CMake / run.sh 默认） |
| 内核 | `f203_keygen_ek_pke_fuse.hpp::FuseEkPke` — AIV0、`runEncode` 后 GM 搬运 |
| Host 输入 | `input/rho.bin`（32B；`gen_data.py` 用 `SEED_RHO=20260620`） |
| Golden | `golden_ek_pke.bin` = `golden_ek_polyvec.bin ‖ golden_rho.bin` |
| G4 编排 | `KEYGEN_ORCHESTRATE=1` 时上游写入 `rho.bin`；单 launch mixPass=4 |

## 验收

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

# 行 21 独立验收（preset dst，跳过 S1–S3）：
NTTS2S1E_MIX_PASS=4 bash run.sh -r cpu -v Ascend910B4
NTTS2S1E_MIX_PASS=4 bash run.sh -r sim -v Ascend910B4
```

`dst` / `t_hat` / `ek_polyvec` / `sk_polyvec` / **`ek_pke`** 与 golden `max_abs_diff=0`。

> **注**：默认 mixPass=0 在 CPU 上 pass5（S2 MMAD）仍会因 `LoadData2D` buffer 尺寸 abort；5→4 第二段 pass4 + 行 21 融合可单独用 `mixPass=4` 验收。G4 编排（`KEYGEN_ORCHESTRATE=1`）由上游预置 `dst_preset` + `rho.bin`，单 launch pass4。

## 与基线关系

| 探针 | 角色 |
|------|------|
| `fix-f203-2s1e-alg13-16171820-k4` | 标量 basemul + 标量 encode；功能基线 |
| **本目录** | **活跃** 向量全链路集成 |
| `byteencode12-vec-k4` / `innerproduct-k4` | 单算子参考 |
| `frozen-…-vec-k4` | 已关闭 v1 half 路线 |
