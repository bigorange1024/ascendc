# TASK-009

## 元数据
- task_id: TASK-009
- issued_at: 2026-09-03T16:50Z
- deadline_min: 60
- max_retries: 1
- silent_hang_min: 12
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-decrypt-hang.yaml
- related_nodes: [D-near-toy, D-next-build-decrypt-toy, D-verify-sim-for-npu, D-sim-enough-before-npu, D-softsync-is-prod-not-homemade, D-forbid-syncall-while-wait, D-reject-correctness-antipattern, J-omit-set4-hangs-decrypt, J-hang-needs-handshake-break, Q-toy-repro]
- hypothesis_under_test: [J-omit-set4-hangs-decrypt, J-hang-needs-handshake-break, Q-toy-repro]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在 `ascendc-tests/` 新建 **Decrypt fused 握手骨架 toy**：默认合法 SoftSync+两轮 GATE+stub Cube 的 SIM 能跑完（magic）；**故意省略 SET(4)** 对照应 SIM 超时 124。不对 ML-KEM 正确性。

## 允许改动范围
- 路径白名单：
  - `ascendc-tests/fix-decrypt-skel-mix-chain-toy/`（新建整树）
  - `ascendc-tests/INDEX.md`（只加一行索引）
  - `graph_tests/_outbox/FB-TASK-009.md`（反馈）
  - 可选：本目录 `STATUS.md`；`graph_tests/INDEX.md` 一行
  - 若用到的 AscendC API 索引无记录：`library/documents/CANN-AscendC算子开发接口参考-查阅索引.md` 顶部追加一行
- 禁止：
  - 改 `examples/stable/**`、Encaps/Decaps/PKE Decrypt 全量、`.cursor/rules|skills`
  - 从 `**/frozen/` 抄码/抄路线
  - 真实 unpack/ByteDecode/su_dot/NTT merge/Keccak（只用 stub：Duplicate / 少量 Adds / 一次轻量 MMAD）
  - AIC Wait 中 `SyncAll`；自造**双向** SoftSync 汇合（必须跟生产：AIV0 写 1、AIV1 自旋）
  - 滥增 Host launch、逐步标量外搬 GM 当「正解」
  - 把 Encrypt skipNtt（无 SoftSync、入口 Wait(4) 后直接 Cube）原样当 Decrypt 拓扑
  - commit/push；再派子 agent；并行 SIM
  - 无限返工超过 max_retries

## 必读材料（按序）
1. `graph_tests/CHARTER.md`、`graph_tests/SUBAGENT_RULES.md`、`graph_tests/DECRYPT_HANG_PLAN.md`
2. 本 TASK 图谱摘录（下表）；完整图 `docs/rg-kem-decrypt-hang.yaml`（只读）
3. 生产握手规格（**只读同步顺序，禁止移植 unpack/NTT 实现**）：
   `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/compute/g4_full/f203_decrypt_device_fused_entry.cpp`
4. 工程壳参考（**只抄壳**）：活跃 `ascendc-tests/fix-encrypt-skel-mix-chain-toy/` 的 `CMakeLists.txt` / `cmake/` / `run.sh` / `aic_func.hpp` / `aiv_func.hpp` / `basic.hpp` / `kyber_limb6.hpp` / 轻量 `tiling.h`
   — kernel `mmad_custom.cpp` 与 `main.cpp` **必须按 Decrypt 握手重写**，不要复制 skipNtt/Hostμ 逻辑。
5. 写 AscendC 前：`library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`（`CrossCoreSetFlag`/`WaitFlag`、`PipeBarrier`、`DataCopy`/`Duplicate` 已有记录则按表实现）
6. `.cursor/skills/ascendc-engineering-notes/SKILL.md`（PIPE / MIX / CrossCore）

### 图谱摘录
| id | kind | status | statement |
|----|------|--------|-----------|
| D-near-toy | decision | active | 轻量 SoftSync+GATE+stub Cube；不对正确性 |
| D-sim-enough-before-npu | decision | active | SIM 沉机制前不请用户上机 |
| F-aic-entry-wait4 | fact | verified | AIC 入口 Wait(4) 再 Set(8) |
| F-softsync-two-slots | fact | verified | AIV0 写哨兵、AIV1 自旋；slot0/1 |
| J-omit-set4-hangs-decrypt | inference | unverified | 合法 SoftSync 后缺 SET(4) ⇒ SIM 挂 |
| J-hang-needs-handshake-break | inference | unverified | 合法 stub 应绿；挂来自握手断裂 |
| J-dual-cube-sufficient-hang | inference | retracted | 勿把双 Cube 当充分 hang 因 |
| Q-toy-repro | question | open | 何种最小骨架能绿/能 124 |
| D-forbid-syncall-while-wait | decision | active | 禁止 AIC Wait 中 SyncAll |
| D-softsync-is-prod-not-homemade | decision | active | 单向 SoftSyncArrive 定式 |

## 步骤（按序，勿跳）
1. 新建目录 `ascendc-tests/fix-decrypt-skel-mix-chain-toy/`（`fix-` 前缀；未绿勿改 `pass-`）。
2. **默认合法握手（v1）** — 单 Host MIX launch（`KERNEL_TYPE_MIX_AIC_1_2`）：
   - Host：分配 `softSyncGm` int32[2]（建议 64B 对齐），**launch 前必须清零**；output 64B。
   - AIV0：stub_prep（UB Duplicate 常量，禁止真 unpack）→ `Arrive(slot0)`。
   - AIV1：`while (s[0]==0);` 后继续。
   - 双 AIV：`SET(4)` → `WAIT(8)` → Clear(slot0)（除非 OMIT_SET4）。
   - NTT-like：flag **1** + 一次轻量 MMAD（16×32×32 即可）+ flag **3**。不要做真 Stage1 split。
   - AIV0：stub_dot（Adds 即可）→ `Arrive(slot1)`；AIV1 自旋 slot1。
   - 双 AIV：再 `SET(4)` → `WAIT(8)` → Clear(slot1)。
   - INTT-like：再 flag **1** + 轻量 MMAD + flag **3**（**不要** Wait/Set flag 2）。
   - AIV0：写 magic 到 out（建议 ASCII `SKELDEC1`，`out[8]` 标记档位）。
   - AIC：入口 `WAIT(4)`→`SET(8)` → `WAIT(1)` MMAD `SET(3)` → 第二轮 `WAIT(4)`→`SET(8)` → `WAIT(1)` MMAD `SET(3)`。
   - `gen_data.py`：任意合法尺寸 input；`verify`：只检查 magic/长度。
   - `run.sh`：接入 `runtime_env.sh`、`camodel_sim_log.sh`；`KERNEL_COMPUTE_BUDGET_SEC` 默认 **180**。
   - 编译开关：`SKEL_OMIT_SET4` 默认 **0**（cmake `-D`，与 encrypt toy 相同接法）。
   - 自研代码写**详细中文注释**（文件头 + 函数头 + 每个同步点）。
3. 验收 SIM（**串行**，不要 CPU 门禁）：
   ```bash
   cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
   # A 合法握手（默认）
   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   # B 故障注入：AIV 两轮都不 SET(4)（或至少第一轮不 SET）；预期 timeout 124
   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
   - A 成功：进程正常结束；verify magic；记录 `wall_sec`；用例根无 stray `core*.dump`。
   - B 成功（对本假说）：rc=**124** 或明确 kernel 超时；**等 timeout 一次即可**，不要杀完不留日志。
   - 若 A 挂：只允许 **1 次**针对性修同步再跑；仍挂 → FEEDBACK `FAIL`，停止。
   - 若 B 不挂：FEEDBACK 写 **refute/weaken** `J-omit-set4-hangs-decrypt`，不要改参数硬凑 124。
4. 更新 `STATUS.md` + `ascendc-tests/INDEX.md` 一行。
5. 写 `graph_tests/_outbox/FB-TASK-009.md`（严格 FEEDBACK 模板）。

若时间不够只跑完 A：outcome=`PARTIAL`，仍须交卷（A 的日志必须有）。优先保证 A 绿再跑 B。

## 验收命令（原样跑，贴完整尾部日志）
```bash
cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 反馈要求
按 `docs/rg/AGENT_TASK_PROTOCOL.md` 的 FEEDBACK 模板写到  
`graph_tests/_outbox/FB-TASK-009.md`。  
必须填图谱影响表（至少 `J-omit-set4-hangs-decrypt` / `J-hang-needs-handshake-break` / `Q-toy-repro`：support/weaken/refute/n/a）。  
失败优先。禁止 commit/push。
