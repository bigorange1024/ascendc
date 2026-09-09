# KGR-P03 — PKE KeyGen L2b Â∘ŝ+ê + ByteEncode₁₂ + ek‖ρ

> **runner**: `subagent_cpu_sim`  
> **npu_owner**: main  
> **实现目录**: `graph-tests/kg_related/RB-K03-kg-dot-encode/`（新建）  
> **上游**: `RB-K01-kg-prep`（Â）、`RB-K02-kg-ntt`（NTT ŝ/ê）— 本刀可 gen_data 自洽复现同 SEED，或读约定 bin 布局

## 测什么（人话）

Alg.13 行 17–21：t̂ ← Â∘ŝ + ê（NTT 域点积加噪声）→ ByteEncode₁₂(t̂)、ByteEncode₁₂(ŝ) → 拼 `ek_pke = BE(t̂)‖ρ`、`dk_pke = BE(ŝ)`。

## 对拍谁

Host / FIPS oracle（内积+BE 契约）；路径写清。本刀可不接 liboqs 全链（留给 P04）；若易接 liboqs ek/dk 亦可作加分交叉。

## 拓扑约束（KB §B2）

- MIX；basename：`kg_dot_encode_custom`（唯一）  
- flag ∈ `{1,3,4}`；`BLOCK_DIM=1`  
- 写出 DataCopy；禁 SetValue→GM  
- **禁止**融 prep/NTT 进本核；禁抄 KeyGen / Encaps/Decrypt 整核  
- 积木：alg11-12 / innerproduct / byteencode **契约**（`-I` 或 shared）；禁大段照抄

## Seed

与 K01/K02 对齐 `SEED_D=20260619`。

## 交付

1. `RB-K03-kg-dot-encode/` + 中文注释同轮  
2. `bash run.sh -r cpu -v Ascend910B4` SUCCESS  
3. sync_audit → `tasks/KGR-P03-kg-dot-encode/logs/sync_audit.json`  
4. STATUS + FEEDBACK；更新 `kg_related/INDEX.md`

## 验收语言

```text
测的是：KeyGen 点积+Encode → ek_pke / dk_pke（或中间 t̂+BE）
对拍：host oracle（路径）
结果：max=0 / FAIL
```

下一刀：KGR-P04 三 launch 全链 + liboqs。
