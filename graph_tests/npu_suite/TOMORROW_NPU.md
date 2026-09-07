# 实机上机 — 下一刀 R×N（多轮粘性）

> **单轮 C0–C2 已全绿（2026-09-07）**。本页只跑多轮。  
> **反馈**：只打字发 `REPORT:` / `SUMMARY`；勿回传文件。

---

## 测什么

| 项 | 内容 |
|----|------|
| 档 | 默认 **C2**（最全：粘合 + Â 2×2） |
| 轮数 | 默认 **7**（对齐历史「第7轮才挂」） |
| 问 | 单轮绿之后，多轮是否出现粘性挂？ |

- 全绿 → 进 **ENCRYPT-GAP**（套件结构 ≠ 旧 Encrypt 挂因）  
- 中途挂 → 记最后 `last=` 与大约第几轮

---

## 怎么测

```bash
git pull --ff-only origin cursor/kem-2launch-sticky-1534
source scripts/env.sh
cd graph_tests/npu_suite
bash run_rxn_npu.sh -v Ascend910B4
```

可选：`TOY_ROUNDS=7 RXN_CASE=C2`（已是默认）。预算默认约 5400s。

---

## 怎么回报

```text
REPORT: C2 PASS last=111
SUMMARY R×N C2x7:OK
```

或挂了：

```text
REPORT: C2 HANG last=110
SUMMARY R×N C2x7:TIMEOUT124
```

（若知道大约第几轮，顺手写 `round≈3` 即可。）
