# Profiling 收口：PKE Decrypt 单 Host launch 融合核

| 项 | 内容 |
|----|------|
| **实验** | ML-KEM / PKE **Decrypt**：Host 只 launch **1** 次的融合 kernel（目录 `graph-tests/dec_related/RB-D09-decrypt-1launch`） |
| **平台** | Ascend 910B3 真机；物理卡 davinci6 → `ASCEND_DEVICE_ID=0`；Freq **1800/1800 MHz（满频）** |
| **时间** | 2026-09-11 |
| **远端全集** | `/mnt/workspace/launch-reduce-logs/pke-decrypt-1launch-prof-20260911-104959/` |

本目录是从远端拉回的**关键可读产物**；完整 dump / OPPROF 树仍在板上。

---

## 做了哪些采集（按实验内容，不按暗号）

### 1. 算子级流水线利用率 + 真实 cycle（成功）

- 工具：`msopprof --aic-metrics=BasicInfo,PipeUtilization,Default`
- 结果：
  - Task Duration ≈ **454.3 µs**
  - **vector0**：813224 cycles，**scalar 占比 ≈ 96.8%**（向量算力几乎空转）
  - **cube0**：722907 cycles，cube 占比 ≈ 0.01%；大量时间在 `scalar_wait_id1`（等跨核同步）
  - **vector1**：723855 cycles，自身 scalar 低，但大量 `scalar_wait_id*` 等 CrossCore
- 产物：`PipeUtilization.csv`、`OpBasicInfo.csv`、`visualize_data.bin`（可进 MindStudio Insight）

**结论（有 cycle 证据）**：该 Decrypt 融合核是 **scalar / 握手等待主导**，不是 Cube/MTE 算力墙。

### 2. 核内指令级 TimelineDetail（失败，已复现钉死）

- 工具：`msopprof --aic-metrics=TimelineDetail --dump=on`
- 现象：kernel 能跑完，但 **dump kernel args 失败** → `Failed to get any available dump file` → **无指令级 JSON/HTML 时间轴**
- 日志：`A_timeline_detail.log`
- 仍落盘了 `visualize_data.bin`（不能替代指令时间轴）

### 3. 应用级 Chrome Trace JSON（成功）

- 工具：`msprof` 采集后 `msprof --export=on --type=text`
- 产物：`msprof_20260911105015.json`（Chrome Trace 事件列表，74 条）
- 内容级别：**Host/Device 任务与 ACL API**（Memcpy、Malloc、kernel `d09_decrypt_fused_custom` 等），**不是** Cube/MTE/Scalar 流水线指令图
- 打开方式：`chrome://tracing` / Perfetto / MindStudio Insight

### 4. cannbot msopprof-visualization HTML 报告（成功，timeline 页省略）

- 入口：`report.html`（约 189 KB）
- 已渲染页：details / roofline / cache / raw-data
- **timeline 页省略**：本机 `msprof op` CLI **无** `PipeTimeline` / `pipeTimeLine` 指标（见 `report_index.json`）
- 另有 `collection_manifest.json`（可视化管线标准入口）

---

## 产物对照（你要看哪张“图”）

| 你想看的 | 有没有 | 文件 / 打开方式 |
|----------|--------|-----------------|
| Host/Device 任务时间轴 JSON | **有** | `msprof_20260911105015.json` → chrome://tracing |
| 算子 Pipe/cycle 表 | **有** | `PipeUtilization.csv` + HTML `report.html` |
| Insight 用 visualize_data.bin | **有** | 远端 OPPROF 目录内 |
| 核内指令流水 JSON/HTML | **无** | TimelineDetail dump 失败 + CLI 无 PipeTimeline |

---

## 与「六档 Launch 压缩」总表的关系

总表见 [`qa/active_npu_perf_summary.md`](../../../qa/active_npu_perf_summary.md)。  
本轮是对该表中 **「PKE Decrypt · 单 Host launch 融合核」** 一行的 **profiling 深采收口**，不是改算法。

## 远程 IDE 连不上时怎么看图

本目录已进 git。在 GitHub 打开本目录即可下载，不必连 Cursor remote server。

**你要的白底时间轴（标准 Chrome Trace JSON）：**

1. 下载 `chrome_trace_wrapped.json`（或 `msprof_20260911105015.json`）
2. 本地 Chrome 打开 `chrome://tracing` → Load 该文件  
   或打开 https://ui.perfetto.dev/ 拖入

截图参考：`chrometracing_overview.png`、`chrometracing_kernel.png`（白底）。

其它：`report.html`（cannbot HTML）、`PipeUtilization.csv`（cycle 表）。

