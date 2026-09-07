# AGENT_HANDOFF

**日期**：2026-09-07  
**分支**：`cursor/kem-2launch-sticky-1534`

## 当前真相

- **NPU_SUITE 单轮全绿（N7）**：C0/C1/C2 均 `PASS last=111`（用户打字回报）。
- 分支 **B3**（`BRANCHING.md`）：套件覆盖结构单轮不粘性挂。
- 下一刀：**R×N（C2×7）**；若仍绿 → ENCRYPT-GAP。
- 反馈：只打字；NPU 只认 Host 1xx。
- ByteDecode/正确性：挂因未明前不做。

## 请用户跑 R×N

```bash
git pull --ff-only origin cursor/kem-2launch-sticky-1534
source scripts/env.sh
cd graph_tests/npu_suite
bash run_rxn_npu.sh -v Ascend910B4
# 默认：仅 C2，TOY_ROUNDS=7
```

打字发回 `REPORT:` / 是否中途 HANG（例 `HANG last=110 round=?`）。
