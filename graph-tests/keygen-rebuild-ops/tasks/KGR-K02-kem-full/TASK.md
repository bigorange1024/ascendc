# KGR-K02 — KEM KeyGen 四 launch 全链 + liboqs

> **runner**: `subagent_cpu_sim`（本机 CPU；SIM 仅 TASK 点名）  
> **npu_owner**: main · **当前云机已关** → 本刀 NPU 标 `wait_npu`，subagent **禁止** SSH/NPU  
> **实现目录**: `graph-tests/kg_related/RB-K06-kem-full/`（新建）  
> **上游砖**: `RB-K01`…`RB-K05`（可链源或同 SEED 自洽）

## 测什么（人话）

Host 串联 **Alg.19 / Alg.16** 全链四 launch：

```text
L1 kg_prep → sync → L2a kg_ntt → sync → L2b kg_dot_encode → sync → L3 kg_kem_tail
→ ek(1568) + dk_kem(3168)
```

`dk_kem = dk_pke‖ek‖H(ek)‖z`（布局契约见 F203 Alg.19 note）。

## 对拍谁

**权威 = `scripts/liboqs_kem_ref` KeyGen**（同 `SEED_D=20260619` + 仓内 `SEED_Z` 域分离约定，与 K01/`liboqs_kem_fixture` 一致）。  
缺 liboqs → **BLOCKED**，禁 python 冒充权威关本刀门禁。

## 拓扑约束

- 四 launch；basename 沿用上游唯一名（`kg_prep_custom` / `kg_ntt_custom` / `kg_dot_encode_custom` / `kg_kem_tail_custom`）  
- L3 **AIV-only**；禁与 L2b 深 CrossCore 融合；flag∈{1,3,4}；`BLOCK_DIM=1`  
- 写出 DataCopy；禁抄 KeyGen / alg19 算子树 / frozen  
- 可 `-I` / 链接 RB-K01…K05；禁 fork examples keygen

## Seed

`SEED_D=20260619`。

## 交付

1. `RB-K06-kem-full/` + 中文注释同轮  
2. `bash run.sh -r cpu -v Ascend910B4` → ek/dk_kem ≡ liboqs_kem_ref max=0  
3. sync_audit → `tasks/KGR-K02-kem-full/logs/sync_audit.json`  
4. STATUS + FEEDBACK；更新 `kg_related/INDEX.md`  
5. FEEDBACK：`npu: wait_npu`  
6. **禁止**改工程 KB/DAG；禁 commit/push

## 验收语言

```text
测的是：KEM KeyGen 四 launch 全链
对拍：liboqs_kem_ref（路径）
结果：ek/dk_kem max=0 | BLOCKED 缺库
```

下一刀：主控开机后 P04/K01/K02 NPU×30；关 `Q-KEM-KG` / `Q-KG-HANG`（及 PKE 门禁若 NPU 齐）。
