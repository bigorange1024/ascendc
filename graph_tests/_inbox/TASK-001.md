# TASK-001

## 元数据
- task_id: TASK-001
- issued_at: 2026-09-03T09:57Z
- deadline_min: 60
- max_retries: 1
- silent_hang_min: 12
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [D-near-toy, D-next-build-toy, D-verify-sim-for-npu, D-sim-enough-before-npu, J-common-mix-flag13, Q-toy-repro, D-forbid-syncall-while-wait, D-softsync-follow-decrypt, D-reject-correctness-antipattern, D-solo-subagent-timebox, D-no-autonomous-push]
- hypothesis_under_test: [Q-toy-repro, J-common-mix-flag13]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在 `ascendc-tests/` 新建 **Encrypt 任务链骨架 toy**：SIM 能正常跑完、墙钟明显快于全量 Encaps；不对 ML-KEM 正确性；为后续逼近卡死假说打底。

## 允许改动范围
- 路径白名单：
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/`（新建整树）
  - `ascendc-tests/INDEX.md`（只加一行索引）
  - `graph_tests/_outbox/FB-TASK-001.md`（反馈）
  - 可选：`graph_tests/INDEX.md` 用例表一行
- 禁止：
  - 改 `examples/stable/**`、Encaps/Decaps 全量、`.cursor/rules|skills`
  - 从 `**/frozen/` 抄码/抄路线
  - 真实大量 Keccak/SHAKE（只用 stub：常量填充 / Duplicate）
  - AIC Wait 中 `SyncAll`；自造 SoftSync 双向汇合
  - 滥增 Host launch、逐步标量外搬 GM 当「正解」
  - commit/push；再派子 agent；并行 SIM
  - 无限返工超过 max_retries

## 必读材料（按序）
1. `graph_tests/CHARTER.md`、`graph_tests/SUBAGENT_RULES.md`
2. 本 TASK 图谱摘录（下表）
3. 工程壳参考（**只抄壳，不抄算法**）：活跃 `ascendc-tests/pass-toy-mix-s123-byteencode-k2/` 的 `CMakeLists.txt` / `run.sh` / `main.cpp` 结构
4. 写 AscendC 前：`library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`（用到的 API 若索引无记录则查 PDF 并追加索引行）
5. `.cursor/skills/ascendc-engineering-notes/SKILL.md`（PIPE / MIX / CrossCore）

### 图谱摘录
| id | kind | status | statement |
|----|------|--------|-----------|
| D-near-toy | decision | active | 轻量骨架；去重哈希；SIM 正常结束且快 |
| D-sim-enough-before-npu | decision | active | SIM 充分后再请用户上实机；目标导向消卡死 |
| J-common-mix-flag13 | inference | unverified | 挂点公因子 MIX + CrossCore flag 1/3 |
| Q-toy-repro | question | open | 何种最小骨架能复现挂或证伪 |
| D-forbid-syncall-while-wait | decision | active | 禁止 AIC Wait 中 SyncAll |
| D-reject-correctness-antipattern | decision | active | 禁碎写 GM / 滥 launch 当推荐 |

## 步骤（按序，勿跳）
1. 新建目录 `ascendc-tests/fix-encrypt-skel-mix-chain-toy/`（`fix-` 前缀；未绿勿改 `pass-`）。
2. **v1 最小可跑**（先通再加）：
   - Host：**优先 1 个 MIX launch**（禁止一上来 3+ launch）。
   - 设备侧串起**形态骨架**（可用 stub 替代真实算法）：
     1. stub_hash/prep（UB 填常量，禁止真 SHAKE）
     2. NTT-like：AIV↔AIC **CrossCore flag 1/3** + 一次轻量 MMAD（小 tiling）
     3. stub_inner（少量向量 Add/Muls 即可）
     4. INTT-like：再次 flag 1/3 + 轻量 MMAD（或明确文档化的同构二次握手）
     5. stub_encode：写固定 magic 到 GM 输出
   - `gen_data.py`：生成任意合法尺寸 input；`verify`：只检查输出 magic/长度，**不要** ML-KEM golden。
   - `run.sh`：接入 `runtime_env.sh`、`camodel_sim_log.sh`；`KERNEL_COMPUTE_BUDGET_SEC` 默认建议 **180**（防挂死，非性能定标）。
   - 自研代码写**详细中文注释**（文件头 + 函数头 + 关键同步点）。
3. 验收（**仅 SIM**，不要以 CPU 为门禁）：
   ```bash
   cd ascendc-tests/fix-encrypt-skel-mix-chain-toy
   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
   成功标准：进程正常结束；verify 通过（magic）；SIM 墙钟记录到 FEEDBACK；用例根无 stray `core*.dump`。
4. 更新 `ascendc-tests/INDEX.md` 一行 + 本目录 `STATUS.md`（SIM 列）。
5. 写 `graph_tests/_outbox/FB-TASK-001.md`（严格 FEEDBACK 模板）。

若编译/SIM 首次失败：只允许 **1 次**有针对性修复再跑；仍失败 → FEEDBACK `FAIL`/`PARTIAL`，列出日志，**停止**。

## 验收命令（原样跑，贴完整尾部日志）
```bash
cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 反馈要求
按 `docs/rg/AGENT_TASK_PROTOCOL.md` 的 FEEDBACK 模板写到  
`graph_tests/_outbox/FB-TASK-001.md`。  
必须填图谱影响表（至少触及 `Q-toy-repro` / `J-common-mix-flag13`：support/weaken/n/a + 说明）。  
未完成也要交卷。禁止 commit/push。
