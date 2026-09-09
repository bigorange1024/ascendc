# KGR-P04 — PKE KeyGen 三 launch 全链 + liboqs

> **runner**: `subagent_cpu_sim`（本机 CPU；SIM 仅 TASK 点名）  
> **npu_owner**: main · **当前云机已关** → 本刀 NPU 标 `wait_npu`，subagent **禁止** SSH/NPU  
> **实现目录**: `graph-tests/kg_related/RB-K04-pke-full/`（新建）

## 测什么（人话）

把已绿砖拼成 **Alg.13 全链**：Host 串联  
L1 `kg_prep` → sync → L2a `kg_ntt` → sync → L2b `kg_dot_encode` → 写出 `ek_pke`(1568) / `dk_pke`(1536)。

## 对拍谁

**权威 = liboqs PKE KeyGen**（同 `SEED_D=20260619` 约定）。缺 liboqs → **BLOCKED**，禁 python 冒充权威关闭本刀门禁。

## 拓扑（KB §B2）

- 三 launch；basename 沿用上游唯一名（或本 binary 内链同一套源，勿撞名）。  
- 禁融成单核；禁抄 stable/pass-fix KeyGen 算子树。  
- 可 **复用/链接** `RB-K01`/`K02`/`K03` 源或 `-I` 其头；**禁止**从 examples keygen fork。

## 交付

1. `RB-K04-pke-full/` + 中文注释  
2. `bash run.sh -r cpu -v Ascend910B4` → ek/dk ≡ liboqs（或明确 cmp 脚本）  
3. sync_audit → `tasks/KGR-P04-pke-full/logs/sync_audit.json`  
4. STATUS + FEEDBACK；更新 `kg_related/INDEX.md`  
5. FEEDBACK 注明：`npu: wait_npu`（云机关机）

## 验收语言

```text
测的是：PKE KeyGen 全链三 launch
对拍：liboqs_pke_ref（路径）
结果：ek/dk max=0 | BLOCKED 缺库
```

下一刀：主控开机后 NPU×30；或并行开 KGR-K01（若 P04 CPU 已绿）。
