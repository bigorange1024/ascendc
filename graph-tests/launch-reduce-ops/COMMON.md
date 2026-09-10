# Launch 压缩 · COMMON（覆盖旧重建「先 CPU」）

继承工程 KB + KeyGen/Decrypt 永禁；本战役增量如下。

## 验收

- **唯一结案 runner**：`bash run.sh -r npu …`（真机号以 `which_npu` 为准）。  
- **禁止**新写 CPU/SIM 测试路径作门禁；既有 `run.sh` 可保留但 **Agent 不得跑 `-r cpu|-r sim` 结案**。  
- 加压：中间 ×30；预算 180s；干净卡。

## 性能记录（对齐工程教材口径）

权威说明：[`docs/research/教材KEM实机测量清单.md`](../../docs/research/教材KEM实机测量清单.md)「多 launch 测准」。本战役填表见 [`PERF.md`](PERF.md)。

| 优先级 | 填什么 | 禁止 |
|--------|--------|------|
| **1 设备真值** | 各 kernel **Task Duration** 求和（教材：`kernel_details` → `[msprof_kernel_total]`；本轮重建：ops-profiling `op_summary` 同行字段） | 终端一行 duration、自造 host wall |
| **2 Host 逐 launch** | `[npu_launch]` / JSONL（有则记） | 用其冒充设备真值 |
| **3 进程墙钟** | `[wall_sec]` 仅对照 | **不得**写入「算子性能」结论格 |

采集工具：

- 教材 / stable KEM：`RUN_WITH_MSPROF=1 MSPROF_MODE=app` + `scripts/npu_msprof_summarize.py`
- 深度瓶颈：`thirdparty/cannbot-skills/ops/ops-profiling`（`msprof_profile_run.sh` + `msprof_perf_summary.py`）→ 用例 `docs/perf/round_*`

数字落盘：**本目录 `PERF.md` + 各用例 `STATUS.md`「性能」节**；原始 PROF 可留远端，摘要数字须进 git。

## cannbot

每刀设备码变更后必跑 sync_audit（见 PLAN §6）；日志进 `tasks/<ID>/logs/`。

## Git

无用户明确「提交/推送」→ 禁止 commit/push。

## 空闲

NPU keepalive 期间刀间空转 **&lt;3 分钟**；否则停心跳放机。

## 禁抄

stable / pass-fix / frozen 的 KeyGen、Decrypt fused、Decaps 核：**禁整树 fork**。  
允许：只读 launch 数与「prep+单 MIX」**模式**；从 `RB-K04`/`RB-D04`/`RB-T26` **重建树**改。
