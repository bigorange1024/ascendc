# KGR-P02 — PKE KeyGen L2a NTT(ŝ/ê)

> **runner**: `subagent_cpu_sim`  
> **npu_owner**: main（本刀不要求 NPU）  
> **实现目录**: `graph-tests/kg_related/RB-K02-kg-ntt/`（新建）  
> **上游**: `RB-K01-kg-prep`（PASS_CPU；布局见其 STATUS）

## 测什么（人话）

Alg.13 行 16：对 prep 产出的 ŝ、ê 做正向 NTT，得到 NTT(ŝ)、NTT(ê)。本刀 **不做** Â∘ŝ、ByteEncode、ek/dk 拼接。

## 对拍谁

Host / FIPS 风格 NTT oracle（与仓内 Tag5T / poly-batch 契约一致即可）；写明路径。可用 K01 同 SEED_D=20260619 生成 ŝ/ê 再 NTT，或本刀 gen_data 自洽链。

## 拓扑约束（KB §B2）

- Launch **MIX**，basename：`kg_ntt_custom`（全局唯一）。  
- CrossCore flag ∈ `{1,3,4}` 仅；`BLOCK_DIM=1`。  
- NTT S1–S3：**禁 Gather**、**禁 limbsplit**（poly-batch：每 AIV 握完整 poly hi+lo）。  
- 业务写出：UB+DataCopy。  
- **禁止**把 prep 融进本核；禁抄 KeyGen / Encrypt/Decrypt 整核。

## 积木参考（契约，禁大段照抄）

- `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2` STATUS / docs/notes MLKEM-NTT  
- toys NTT LAYOUT  
- K01 输出布局：`ŝ[4,256]` / `ê[4,256]` int32；`a_hat` 本刀可不消费

## 交付物

1. `RB-K02-kg-ntt/` 完整用例 + 中文注释同轮  
2. `bash run.sh -r cpu -v Ascend910B4` SUCCESS  
3. `logs/sync_audit.json`  
4. STATUS.md + FEEDBACK.md；更新 `kg_related/INDEX.md`

## 验收语言

```text
测的是：KeyGen NTT(ŝ) 与 NTT(ê)
对拍：host/FIPS NTT oracle（路径）
结果：max=0 / FAIL
```

## 下一刀

PASS → KGR-P03（dot+BE+ek/dk）。
