# AGENT_HANDOFF

**日期**：2026-09-06  
**分支**：`cursor/kem-2launch-sticky-1534`

## 当前真相

- Encrypt 实机粘性挂根因未钉死；SIM 不能复现粘性挂。
- **上机反馈硬约束**：用户**不能回传任何文件**，只能打字发三位编码。禁止再要 log/模板文件。
- ByteDecode / 正确性：卡死点之后，挂因未明前不做。
- E01–E16 + `graph_tests/npu_suite/` 已齐。

## 明天上机

用户按 `graph_tests/npu_suite/TOMORROW_NPU.md` 跑，聊天打字例如：

```text
C0 PASS
C1 HANG 110
C2 SKIP
```

主控按 `BRANCHING.md` 选下一刀。
