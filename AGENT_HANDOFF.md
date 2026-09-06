# AGENT_HANDOFF

**日期**：2026-09-06  
**分支**：`cursor/kem-2launch-sticky-1534`

## 当前真相

- Encrypt 实机粘性挂根因未钉死；SIM 不能复现粘性挂。
- **上机反馈硬约束**：用户**不能回传任何文件**，只能打字发三位编码。禁止再要 log/模板文件。
- ByteDecode / 正确性：卡死点之后，挂因未明前不做。
- E01–E16 + `graph_tests/npu_suite/` 已齐。

## 实机要测什么

三档阶梯（找挂点，不验正确性）：

| 档 | 内容 |
|----|------|
| C0 | 2-launch + SET(4) 空壳握手 |
| C1 | Encrypt 形态粘合（无 Â SampleNTT） |
| C2 | C1 + Â 2×2 SampleNTT（独立 launch） |

成功关键点：Host `111`。历史主挂征象：有 `110` 无 `111`。

## 怎么测 / 怎么回报

一页纸：`graph_tests/npu_suite/TOMORROW_NPU.md`

```bash
git checkout cursor/kem-2launch-sticky-1534 && git pull --ff-only
source scripts/env.sh
cd graph_tests/npu_suite && bash run_all_npu.sh -v Ascend910B4
```

聊天打字例如：

```text
C0 PASS
C1 HANG 110
C2 SKIP
```

主控按 `BRANCHING.md` 选下一刀。
