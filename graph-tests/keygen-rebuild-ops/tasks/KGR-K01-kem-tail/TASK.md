# KGR-K01 — KEM KeyGen L3 `kg_kem_tail`（H(ek)+z+dk_kem）

> **runner**: `subagent_cpu_sim`（本机 CPU；SIM 仅 TASK 点名）  
> **npu_owner**: main · **当前云机已关** → 本刀 NPU 标 `wait_npu`，subagent **禁止** SSH/NPU  
> **实现目录**: `graph-tests/kg_related/RB-K05-kem-tail/`（新建）  
> **上游**: `RB-K04-pke-full`（或同 SEED 自洽生成 ek_pke/dk_pke）

## 测什么（人话）

Alg.19 / Alg.16 增量尾段（AIV-only）：给定 PKE 产出的 `ek`(1568)+`dk_pke`(1536)，算 `H(ek)`（SHA3-256）、取/派生 `z`(32)，拼

```text
dk_kem(3168) = dk_pke(1536) ‖ ek(1568) ‖ H(ek)(32) ‖ z(32)
```

（布局以 liboqs / `F203-KEM-Alg19-KeyGen设备全链技术总结` 为准；**禁止**抄 KeyGen 算子树。）

## 对拍谁

- 本刀优先：**host oracle**（SHA3-256(ek) + 约定 z 派生 + 拼接）对 `H(ek)` / `dk_kem` 布局。  
- z 派生对齐仓内约定：`exp-mlkem-f203-kem-k4:SEED_Z={SEED_D}`（见 `scripts/liboqs_kem_fixture.py`），`SEED_D=20260619`。  
- 全链 `liboqs_kem_ref` 留给 **KGR-K02**；本刀若易接 liboqs 可作加分交叉，非关闸条件。

## 拓扑约束（工程 KB §5 / S0）

- basename：`kg_kem_tail_custom`（唯一）  
- **AIV-only**；`BLOCK_DIM=1`；**禁止**与 L2b 深 CrossCore 融合  
- 写出 DataCopy；禁 SetValue→GM  
- **禁止**抄 `*keygen*` / alg19 KeyGen 算子树 / frozen；可复用 `library/shared` SHA3 契约与 Encaps/Decrypt **哈希思路**（禁整核 fork）  
- 勿改 P01–P04 已绿核语义；可 `-I` / 读 bin

## Seed

`SEED_D=20260619`（与 P 系列一致）。

## 交付

1. `RB-K05-kem-tail/` + 中文注释同轮  
2. `bash run.sh -r cpu -v Ascend910B4` → H(ek)/dk_kem（或声明的中间量）max=0  
3. sync_audit → `tasks/KGR-K01-kem-tail/logs/sync_audit.json`  
4. STATUS + FEEDBACK；更新 `kg_related/INDEX.md`  
5. FEEDBACK 注明：`npu: wait_npu`  
6. **禁止**改工程 KB/DAG（主控回收）；禁 commit/push

## 验收语言

```text
测的是：KEM KeyGen L3 尾段 H(ek)+z → dk_kem
对拍：host oracle（路径）[+ 可选 liboqs]
结果：max=0 | FAIL
```

下一刀：KGR-K02 四 launch 全链 + liboqs_kem_ref。
