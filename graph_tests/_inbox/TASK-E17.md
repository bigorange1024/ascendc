# TASK-E17 — toy-e17-l18-fsm-reuse13

**图谱**：`D-exp-e17` · 规格总表：`graph_tests/ENCRYPT_GAP.md`  
**deadline_min**: 40  
**max_retries**: 1  
**silent_hang_min**: 10  
**abort_on**: [timeout, no_progress, scope_breach, sim_hang]

---

## 目标（单因子）

新建 `graph_tests/toys/toy-e17-l18-fsm-reuse13/`：用 **stub** 复现 stable Encaps `l18_l19` 的 **CrossCore 同步序**（非业务）：

```
伪 NTT：AIV SET(1) ↔ AIC Wait(1)；AIC SET(3) ↔ AIV Wait(3)
GATE：  双 AIV SET(4) ↔ AIC Wait(4)；AIC SET(8) ↔ 双 AIV Wait(8)
伪 INTT：再次 AIV SET(1) ↔ AIC Wait(1)；AIC SET(3) ↔ AIV Wait(3)   ← 复用 1/3
```

- **单 launch** MIX（`KERNEL_TYPE_MIX_AIC_1_2`）；无真 Cube/NTT/INTT/at_jp。  
- Host：同进程 `TOY_ROUNDS`（默认 **8**）轮；三位数字 TRACE（沿用 E01 风格 `1xx`；设备侧可用 `4xx/5xx` 标阶段）。  
- 写极简 magic 证明跑完（可仿 E01）。  
- **禁止**：抄 Encrypt/l18 业务码；「仅 GATE」空壳；OMIT 对照本刀不做；改图谱 yaml；commit/push；上机；并行 SIM。

参考骨架（只抄工程壳，不抄业务）：`graph_tests/toys/toy-e01-2launch-set4-trace-repeat/`（cmake/run.sh/main 可改编为单 launch）。

只读对照 FSM（**不复制实现**）：  
`examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/compute/f203_encrypt_l18_l19_kernel.cpp` 文件头注释 + `FsmWait`/`FsmSet` 与 AIC/AIV 序。

写码前读：`thirdparty/cannbot-skills/ops/ascendc-api-best-practices/references/api-crosscore-sync.md`（flag 0–15；禁 Wait 中 SyncAll）。

---

## 白名单

- 仅可写：`graph_tests/toys/toy-e17-l18-fsm-reuse13/**`  
- 可写 FEEDBACK：`graph_tests/_outbox/FEEDBACK-E17.md`  
- 可读：E01、ENCRYPT_GAP.md、SUBAGENT_RULES.md、只读 l18、cannbot sync 文档  

---

## 验收

1. `cd graph_tests/toys/toy-e17-l18-fsm-reuse13 && SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`  
   - 默认 ≥8 轮；Host 见成功码（建议末码 `111` 或文档写明等价）  
   - kernel 未 124  
2. `python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py mmad_custom.cpp --check all --format json`  
   - 输出放到 `/opt/cursor/artifacts/e17-sync-audit.json`  
   - SYNC-03 同侧可记假阳性候选；若有真死等红线须在 FEEDBACK 标明  
3. `STATUS.md` + `TRACE.md`（中文注释达仓规：文件头/函数头/关键块）  

本线验收：**SIM only**（勿强跑 CPU）。

---

## FEEDBACK 必填

- 路径、命令、是否 PASS、轮次、kernel wall  
- sync_audit 摘要  
- 对 `D-exp-e17` / `F-encrypt-gap-inventory`：support / weaken / blocked  
- 未做 E18（下一单）  
