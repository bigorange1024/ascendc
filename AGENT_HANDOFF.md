# AGENT_HANDOFF

**日期**：2026-09-07  
**分支**：`cursor/kem-2launch-sticky-1534`

## ★ 60 秒上手（本分支）

1. 分支：**`cursor/kem-2launch-sticky-1534`**（≠ PR#19 / ≠ main 教材主线叙事）。  
2. Cloud 首次：`bash scripts/clone-thirdparty.sh`（含 liboqs；另可 `ONLY=cannbot-skills`）。  
3. 工作方式：知识库 + `rg-encrypt-npu-hangfree` + **cannbot-skills 就地读用**（不 vendor 进 `.cursor/skills`）。  
4. 先读本文件 → `docs/notes/Encrypt-实机无卡死-知识库.md` → `graph_tests/ENCRYPT_REWRITE_PLAN.md` §0.1。  
5. 写 AscendC 前：Rule + cannbot `ascendc-sync-audit` / `ascendc-api-best-practices` + `ascendc-engineering-notes`。  
6. **真机 NPU（GitCode CANNLab，Cursor Agent 远程驱动）**：见 [`docs/engineering/CANNLab接入与远程驱动.md`](docs/engineering/CANNLab接入与远程驱动.md)。需 Secrets `TAILSCALE_AUTHKEY`(reusable)+`CANNLAB_SSH_KEY`；CANNLab 开机后 WebIDE 跑一次 `scripts/cannlab/agent_bootstrap.sh`；新 Agent 侧起 tailscale(userspace)+写私钥+`ssh … developer@cannlab-npu -p 2222`。**单卡实例必须 `ASCEND_DEVICE_ID=0`**；停机用 `sudo kill -TERM 1` 或控制台“关机”（容器无 systemd，poweroff 无效）。本分支上机默认跑 `graph_tests/npu_suite/`（R×N 等），**须用户当次授权**；反馈只打字 `REPORT:`。

## 工作方式（用户锁）

- **本分支 ≠ 其它分支**（勿跟 PR#19 的 T01–T07 / N0–N10 混叙事）。
- **充分使用** `thirdparty/cannbot-skills` 做 Encrypt **从头**开发的 AscendC 工程层（同步/卡死/API/环境/tiling…）。
- **不**整仓拷进 `.cursor/skills/`，**不做** vendor 草案；就地读 `ops/<skill>/SKILL.md` 并跑其脚本。
- 领域路线仍以本仓知识库 + `rg-encrypt-npu-hangfree` + `BRANCHING` 为准。
- 对照表见 `graph_tests/ENCRYPT_REWRITE_PLAN.md` §0.1。

## 当前真相

- **stable 已恢复为 `origin/main` 口径**（本分支上 Hostμ / 真2-launch 等 stable 改动已撤回；只读对照 + 上机取证用 main stable）。
- **CANNLab 运行环境**：`npu_device_map.sh` 支持 `CANNLAB=1` / `NPU_SINGLE_CARD=1` → 全树默认 device **0**；取证脚本 `scripts/cannlab/hang_observe_encaps_decaps.sh`。
- **知识库 + 图谱已 v2 从头刷新（cannbot 中心）**：
  - 库：`docs/notes/Encrypt-实机无卡死-知识库.md`（§0 图谱+cannbot 双开；§0.1 Skill 入口；§3.3 sync_audit SYNC-03 假阳性纪律）。
  - 图：`docs/rg-encrypt-npu-hangfree.yaml` + `.html`；`rg_validate` **OK**；升格 `D-graph-and-cannbot`；下一刀 `D-next-rxn-or-gap`。
  - cannbot 基线：已对 e01/e13/e15 + 只读 l18 跑 `sync_audit.py`（产物在 `/opt/cursor/artifacts/sync-audit-*.json`）。
- **已从 main 合入 CANNLab 联机**：`docs/engineering/CANNLab接入与远程驱动.md` + `scripts/cannlab/{agent_bootstrap,agent_watchdog}.sh`。
- NPU_SUITE 单轮 C0–C2 全绿（N7）→ B3。
- 910B3（CANNLab）**有时限 → 未经用户明确授权不得连实机**；连则 **device id=0**。
- 上机优先：~10min **亲自**多轮/交叉跑 stable Encaps/Decaps 看粘性挂样貌（§5.1）；再 R×N。
- 反馈：只打字三位码 / `REPORT:`；挂因未明前不做 ByteDecode/正确性。

## 下一动作（待用户令 / Secrets）

1. Cloud Secrets 须含 `TAILSCALE_AUTHKEY` + `CANNLAB_SSH_KEY`（本 Agent 环境当前**仅**注入了 `ASCENDC_GH_PAT`）→ 用户开机 CANNLab 并跑 bootstrap → Agent 接入后跑 `hang_observe_encaps_decaps.sh`。  
2. 授权后也可跑 R×N；写码前 cannbot audit，回写图谱；跑完控制台停机。  
3. 无卡时 → ENCRYPT-GAP / SIM 短刀。
