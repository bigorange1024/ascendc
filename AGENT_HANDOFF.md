# AGENT_HANDOFF

**日期**：2026-09-07  
**分支**：`cursor/kem-2launch-sticky-1534`

## 工作方式（用户锁）

- **本分支 ≠ 其它分支**（勿跟 PR#19 的 T01–T07 / N0–N10 混叙事）。
- **充分使用** `thirdparty/cannbot-skills` 做 Encrypt **从头**开发的 AscendC 工程层（同步/卡死/API/环境/tiling…）。
- **不**整仓拷进 `.cursor/skills/`，**不做** vendor 草案；就地读 `ops/<skill>/SKILL.md` 并跑其脚本。
- 领域路线仍以本仓知识库 + `rg-encrypt-npu-hangfree` + `BRANCHING` 为准。
- 对照表见 `graph_tests/ENCRYPT_REWRITE_PLAN.md` §0.1。

## 当前真相

- NPU_SUITE 单轮 C0–C2 全绿（N7）→ B3。
- 910B3 云主机用户已调通；**有时限 → 未经用户明确授权不得连实机**。
- R×N / 下一编码刀：等用户下令；挂因未明前不做 ByteDecode/正确性。
- 反馈：只打字三位码 / `REPORT:`。

## 下一动作（待用户令）

1. 授权上机 → R×N 或指定刀；写码前先跑 cannbot `ascendc-sync-audit`。  
2. 授权继续 SIM 编码 → 按计划 §0.1 打开对应 cannbot Skill 再动 toys/enc。  
3. 暂停则只维护库/图，不连 910B3。
