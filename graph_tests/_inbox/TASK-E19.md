# TASK-E19 — toy-e19-early-entry-trace

**图谱**：`D-exp-e19` · 规格：`graph_tests/EARLY_EMPTY_TRACE.md`  
**deadline_min**: 35  
**max_retries**: 1  
**silent_hang_min**: 10  
**abort_on**: [timeout, no_progress, scope_breach, sim_hang]

---

## 目标

新建 `graph_tests/toys/toy-e19-early-entry-trace/`：

在 **任何 CrossCore Wait/业务** 之前，AIC 与 AIV0 立刻写 fused-trace 风格槽位（int32 GM 数组），证明「入口标」可被 Host 轮询看到。

建议形态（仿 E01/E17 工程壳 + Encaps Host 轮询思路的简化版）：

1. Host 分配 `traceDev[16]`，launch 传入；Sync 期间可每 100–500ms D2H 打印（可简化：核结束后读一次即可，SIM 验收）。  
2. Kernel MIX AIC_1_2：  
   - **入口**：AIV0 `trace[15]=1`（或约定 entry 槽）；AIC `trace[14]=1`（入口）；**禁止**先 Wait。  
   - 然后可选跑极简 SET4 握手（或 E17 缩略）保证能结束。  
3. 默认 `TOY_ROUNDS≥8` SIM。  
4. 中文注释；禁抄 Encrypt 业务（禁 PrefixEmbedMu/NTT 真链）。

---

## 白名单

- 仅：`graph_tests/toys/toy-e19-early-entry-trace/**`  
- FEEDBACK：`graph_tests/_outbox/FEEDBACK-E19.md`  
- 可读：E01/E17、`EARLY_EMPTY_TRACE.md`、`acl_session.hpp` 中 MaybeTrace 注释  

禁止：改 Encaps/stable；改图谱；commit/push；上机。

---

## 验收

`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → rounds 绿；文档写明入口槽语义。  
可选：核结束 Host 打印 `stages set≥2` 含入口槽。

## FEEDBACK

`D-exp-e19` support/weaken；与 EARLY H-E3/H-E1 的关系一句话。
