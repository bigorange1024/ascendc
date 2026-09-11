# 2026-09-11 · PKE Decrypt 单 launch 融合核 profiling 收口

## 决策 / 结论

- **实验**：PKE Decrypt、Host 仅 1 次 launch 的融合核（`graph-tests/dec_related/RB-D09-decrypt-1launch`）。
- **profiling 收口**：应用级 Chrome Trace JSON + cannbot HTML（details/roofline/cache）+ PipeUtilization cycle **已落盘**。
- **核内指令 Timeline**：`TimelineDetail` dump kernel args 失败（复现）；本机 CLI 无 `PipeTimeline` → **无指令级时间轴**，属工具链限制，不是漏采。
- **瓶颈证据**：vector0 scalar ≈ **96.8%** → scalar/跨核握手等待主导。
- 汇报约定：用**实验全名**，不用 K07/D09 等暗号当主称谓。

## 产物

- 本地：`docs/perf/round_003_pke_decrypt_1launch/`（`README.md`、`report.html`、`msprof_*.json`、CSV）
- 总表：`qa/active_npu_perf_summary.md` 已改实验全名并写收口节
- 远端：`/mnt/workspace/launch-reduce-logs/pke-decrypt-1launch-prof-20260911-104959/`

## 遗留

- 若要动性能：下一刀应是改 Decrypt 融合核 scalar/握手实现，而不是继续采同一张缺失的指令时间轴。
- 板仍在线；断板需用户指令。
