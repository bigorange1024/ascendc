# 2026-09-09 · reasoning-graph-skill 入 thirdparty

## Drive 包

- 链接：https://drive.google.com/file/d/16TwSu0JPFqL7eVUAAa9CURpmBq8kj0Cp/view?usp=sharing  
- file id：`16TwSu0JPFqL7eVUAAa9CURpmBq8kj0Cp`  
- 内容：`reasoning-graph-skill-master`（推理图谱 skill，非 cann-ntt 作者包）  
- 落点：`thirdparty/reasoning-graph-skill/` + zip `thirdparty/reasoning-graph-skill-master.zip`  
- 来源说明：`thirdparty/reasoning-graph-skill/SOURCE.md`  
- 登记：`docs/engineering/thirdparty-本地依赖.md`（Drive 手工包）  

## 用途

积累/维护本仓推理知识图谱（`docs/rg-*.yaml` 等）时读 `thirdparty/reasoning-graph-skill/SKILL.md`；**不** vendor 进 `.cursor/skills`。

## 说明

同机已有 `cann-ntt-author-merged_dsa`（另一 Drive/作者包）；本 zip md5 与作者包不同，勿混淆。

## 同日追加：`rg-encrypt-cann-ntt` 迁 skill 骨架

- 文件：`docs/rg-encrypt-cann-ntt.yaml`（`kind`/`deps`/`status`/`config`；约束 → `D-*`）
- 硬校验：`python3 thirdparty/reasoning-graph-skill/scripts/rg_validate.py --yaml docs/rg-encrypt-cann-ntt.yaml` → OK  
  兼容：`python3 scripts/check_rg_dag.py --yaml docs/rg-encrypt-cann-ntt.yaml` → OK（42 nodes）
- **闭合**：`Q-ULT-NOHANG` ← `I-HOST-ORCH-NPU-NOHANG`（EN10–EN12 + `F-X41-SETDEVICE-LOGICAL0`）
- **仍 open**：`Q-CORRECTNESS-FULL`、`Q-OLD-L18-STILL-HANG`
- 软审计：约 12 WARN（多为 EN01–09 evidence 未指到具体 `.log`；`ML-KEM-1024` 触发 observation 假阳性）
- 渲染：`/opt/cursor/artifacts/rg-encrypt-cann-ntt.html`
- KB：`docs/notes/Encrypt-cann-ntt-kb.md` §7 已刷新
