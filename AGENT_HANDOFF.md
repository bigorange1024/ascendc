# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-10（fe53：同步 main 的 HiDevLab WebIDE 脚本/手册；本线仍为 Encrypt×cann-ntt）  
> Git：本线文档可提交推送；`thirdparty/` 本身不进仓

## 依赖

| 项 | 路径 |
|----|------|
| 推理图谱 skill | `thirdparty/reasoning-graph-skill/SKILL.md`（Drive `16TwSu0…`；见 `SOURCE.md`） |
| 登记 | `docs/engineering/thirdparty-本地依赖.md` → Drive 手工包 |
| Encrypt×cann-ntt DAG | `docs/rg-encrypt-cann-ntt.yaml` |
| KB | `docs/notes/Encrypt-cann-ntt-kb.md` §7 |
| HiDevLab WebIDE | [`docs/engineering/HiDevLab-WebIDE操作手册.md`](docs/engineering/HiDevLab-WebIDE操作手册.md) · `scripts/hidevlab/` · 开机 `bash /workspace/hidevlab_boot.sh` |

积累/维护 `docs/rg-*.yaml`：**先读该 SKILL**；勿复制进 `.cursor/skills`。

## 真机 / 上轮结论

- Encrypt×cann-ntt：EN10–EN12 NPU **PASS-NOHANG**（详见 KB）；此前 NPU 作业曾停。  
- HiDevLab（新）：WebIDE 主路径；`add_custom` NPU 冒烟 **PASS**；机上常无法访问 GitHub（勿依赖 `git pull`）。

## 图谱状态（2026-09-09）

- `rg_validate` / `check_rg_dag`：**OK**（42 nodes）
- `Q-ULT-NOHANG`：**answered** ← `I-HOST-ORCH-NPU-NOHANG`
- 仍 **open**：`Q-CORRECTNESS-FULL`、`Q-OLD-L18-STILL-HANG`
- 渲染：`/opt/cursor/artifacts/rg-encrypt-cann-ntt.html`

## P0

按用户下一指令；HiDevLab 开机口令：`bash /workspace/hidevlab_boot.sh`。
