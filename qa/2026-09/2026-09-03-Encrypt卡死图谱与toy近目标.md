# 2026-09-03 — Encrypt 卡死：推理图谱初始建图与 toy 近目标

关键字：**rg-encrypt-l18** · **Q-ULT NPU 不卡+正确** · **D-NEAR toy 骨架** · **主控图谱 / subagent 干活** · **graph-tests 授权** · 禁全量 SIM 磨 FSM

---

## 1. 目标分层（用户确认）

| 层 | 内容 |
|----|------|
| **最终（用户）** | NPU 实机 Encrypt 不再卡在 `l18_l19` SynchronizeStream，且正确性通过 |
| **当前近目标（本阶段 Agent）** | `ascendc-tests` 轻量 toy：模仿 Encrypt 任务链骨架（桩哈希 + NTT/INTT/内积/encoding），不对算法正确性；能跑完且 **SIM 快**；打通「图谱推理 → 实验 → 刷新」流程并沉淀正确知识 |

## 2. 图谱

- 文件：[`docs/rg-encrypt-l18.yaml`](../../docs/rg-encrypt-l18.yaml)
- 校验：`rg_validate` **OK**（约 48 节点；audit 仅 WARN）
- domain：`goals` / `impl-tech` / `verify-test` / `perf-opt`
- 已收入 sticky 分支证伪与失败 decision（SyncAll / SoftSync / INTT 5/7 / 拆双 Cube 当充分修复）

## 3. 工作方式（用户拍板）

- 主控：定目标、沉淀/刷新/推理图谱、设计实验、收反馈
- subagent：按任务模板实现与跑测，按反馈模板回报
- 试验场：`ascendc-tests/<toy>/` + 根目录 **`graph-tests/`**（已授权新建）
- 本阶段禁以改 stable 全量 Encrypt + 反复全链 SIM 为主手段

## 4. 下一刀（未开工）

1. 从图开放问题 `Q-TOY-NTT` / `Q-TOY-GATE-INTT` 设计第一版 toy 规格  
2. 下发 subagent 建探针 + CPU/`SIM_DIRECT` 跑通  
3. 按结果刷新图谱（证伪则整链改 status）

## 5. 图谱治理口径（同日追加）

用户要求：

1. **只服务 debug**：无关知识不进图，防污染。  
2. **禁「为绿而绿」推荐知识**：逐步 GM 外搬、堆 Host launch、放弃融合等严重伤性能做法，即使测绿也**不得**沉淀为推荐实现（见 `D-RG-NO-SLOW-CORRECTNESS`、`D-CPU-MULTI-LAUNCH-TWIN-ONLY`）。  
3. **失败一等公民**：失败实验/经验必须充分总结进图（`fail-lessons` / inactive），禁止只堆通过项（`D-RG-FAIL-MUST-KEEP`）。

已落地：`rg-hygiene` + `fail-lessons` 域；HTML 已重渲。

## 6. SIM 为主 / CPU 不进图（同日追加）

用户：最终要 **NPU 实机跑通**；过程验证以 **SIM 为关键**。**CPU 测试及相关经验无参考价值**，不得再作图谱推理依据。

已落地：`D-RG-SIM-PRIMARY`；`F-CPU-LAUNCH` / `F-CPU-MULTI-GREEN` → `retracted`；toy 开放问题改为仅问 SIM。

## 7. graph-tests 目录已建（同日）

根目录 [`graph-tests/INDEX.md`](../../graph-tests/INDEX.md)：把目标分层、图谱治理、失败必沉、SIM 为主/CPU 不采信、主控↔subagent 模板、下一刀顺序写死，防遗忘。

## 8. Subagent 时限 / 单并发 / 禁自主 push（同日）

- **不得自主 push**（主控与 subagent 皆然）。  
- **同时只 1 个**干活 subagent；禁并行 SIM。  
- 下发必带：`WALL_CLOCK_BUDGET` / `MAX_SIM_ATTEMPTS=2` / `MAX_REWORK_ROUNDS=1` / `DONE_DEFINITION` / `ABORT_IF`；到点 ABORT，禁止傻等与无限返工。  
- 细则：[`graph-tests/INDEX.md`](../../graph-tests/INDEX.md) §4.3–4.4。

## 9. 终点是 Encrypt 卡死解决；实机拷贝稀缺（同日）

- 实验（toy / graph-tests / 图谱）**达成标准** = 能支撑解决 Encrypt 运行卡死（最终 NPU），不是 toy 自绿。  
- 用户可帮忙上机，但**不能随时拷** → 须先 SIM 实验充分 + §1.1 清单，再打包请上机。  
- 见 [`graph-tests/INDEX.md`](../../graph-tests/INDEX.md) §1 / §1.1 / §6。

## 10. 跨分支沉淀（同日）

用户：知识来源**不限当前分支**；其它分支刚做的实验笔记也要沉淀。

- 治理：`D-RG-MULTI-BRANCH-SRC`；`graph-tests/INDEX.md` §2.0  
- 已从 `origin/cursor/kem-2launch-sticky-1534` 补：`F-SKIP-REBUILD-OLD-FUSED`、`F-RESET-BEYOND-DEVICEGUARD`、`J-FAIL-BAD-DEVICE-CARD`、`J-FAIL-PREP-FORBIDDEN-IN-MIX`（此前图里已有大量 sticky 09-02/03 内容，本次补缺口）

## 11. GT-1 PASS → GT-2（同日）

- GT-20260903-1：`fix-toy-encrypt-fsm-ntt1` SIM PASS → `Q-TOY-NTT` answered / `F-TOY-NTT1-SIM-PASS`
- 已下发 GT-20260903-2：GATE+INTT 1/3（同时仅 1 subagent）

## 12. GT-2 PASS → GT-3 融合骨架（同日）

- GT-2：GATE+INTT SIM 绿，但**跳过同核 NTT**（INTT 独占 1/3）
- 图谱：`Q-TOY-GATE-INTT` answered；新问题 `Q-TOY-FUSED-L18-SKEL`
- GT-3：同核 NTT→GATE→INTT + TRACE（逼近 l18，服务空 TRACE）

## 13. GT-3 融合骨架绿；TRACE 假空 → GT-4（同日）

- 同核 NTT→GATE→INTT SIM PASS；AIV0 TRACE 可见；**AIC 标量写 GM TRACE 假空**（`J-FAIL-AIC-SCALAR-TRACE`）
- 实机 0/16 更宜解释为 AIV0 未到首 mark（条件结论）；先把 TRACE 改 DataCopy 再谈上机

## 14. GT-4 首派拦截后重派（同日）

- 首派因 provider usage guidelines 失败（非技术）；改写英文中性任务单后重派 DataCopy TRACE。

## 15. GT-4 PASS；曾拟上机（同日）

- TRACE DataCopy：SIM 上 AIV0 `0–3` + AIC `5/6` 可见；`Q-TOY-TRACE-DATACOPY` answered。
- 曾写偶发上机清单；**用户订正：暂时无法上机（明天）**；toy 要在 **SIM 多跑加压**看卡死。

## 16. 改回 SIM 加压 · GT-5 多 launch（同日）

- `D-NPU-TRIP-TOY-SKEL1` → inactive（推迟）；新开 `Q-TOY-SIM-MULTI-LAUNCH-HANG` + GT-5。

## 19. Decrypt 独立图谱 + SoftSync toy（同日）

用户订正：不得把 Decrypt 卡死挂在 Encrypt 线后面空等上机。须：

- 独立图谱 [`docs/rg-decrypt-fused.yaml`](../../docs/rg-decrypt-fused.yaml)
- 试验场 [`graph-tests/decrypt/`](../../graph-tests/decrypt/INDEX.md)
- 像 Encrypt GT 一样做 toy、刷新图谱；主控指挥、1 个 subagent 干活

已建图（`rg_validate` OK）。独有挂点假设：`SoftSyncArrive`（AIV1 `while(s[slot]==0)`）。Encrypt GT-1..7 未覆盖。Cloud SIM 上 stable prod input-only **未复现挂死**。

**DGT-1..4 均 SIM PASS**（SoftSync → Soft+GATE → 融合骨架 → 16 launch）。toy 尺度 **未复现挂死**（`J-DECRYPT-TOY-SIM-NO-HANG-SO-FAR`）。未催上机。

失败已沉：fused 内 TPipe mark 破坏对拍；`F203_DECRYPT_TRACE=1` SIM 轮询污染 golden（同 Encrypt `F203_L18_TRACE`）。


- `TOY_SPLIT_2LAUNCH=1` ×16×2 进程 SIM PASS；`Q-TOY-SIM-2LAUNCH-HANG` answered。
- 条件结论：`J-TOY-SIM-NO-HANG-SO-FAR`——toy 尺度 SIM 暂未复现挂死；真卡死更可能在生产 Encrypt/NPU。
- 下一刀待用户定（继续逼近生产路径 / 或明日上机）。

