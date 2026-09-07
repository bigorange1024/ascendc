# AGENT_HANDOFF

**日期**：2026-09-06  
**分支**：`cursor/kem-2launch-sticky-1534`

## 当前真相

- Encrypt 实机粘性挂根因未钉死；SIM 不能复现粘性挂。
- **上机反馈硬约束**：用户**不能回传任何文件**，只能打字发 `REPORT:` / 三位编码。
- **NPU 只认 Host 1xx**：设备 `200/400` 常不可见；旧报 `L1 TRACE 200 missing` 在 Host 已有 `111` 时 **不是粘性挂**。
- ByteDecode / 正确性：挂因未明前不做。

## 用户 2026-09-07 回报解读

- 未见醒目 C0/C1/C2（旧 banner 弱 + 编译刷屏）；见 `NTT Test`（gen_data 噪音）后 `[FAIL] L1 TRACE 200 missing`。
- 该 FAIL 出现前 verify **已通过 Host 序列**（含 `111`）→ **C1 未粘性挂**；假红来自设备 TRACE。
- 已修：Host-only verify + 大字 `REPORT:` + 失败继续跑完三档。

## 请用户再跑一版

`git pull` 后 `bash graph_tests/npu_suite/run_all_npu.sh -v Ascend910B4`，打字发回所有 `REPORT:` / `SUMMARY` 行。
