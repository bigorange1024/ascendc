# TASK E02（草稿 — E01 关门后再下发）

## 元数据（预定）
- task_id: E02
- deadline_min: 30
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-softsync-follow, D-set4-invariant, D-short-experiments]
- write_graph: no

## 目标（拟定）
新目录 `toys/toy-e02-softsync-then-set4/`：在 E01 砖上，双 AIV 先 SoftSyncArrive 再 SET(4)；SIM 绿；禁止改 E01 目录（复制或新写）。

## 验收（拟定）
- 默认 SIM 绿（可 3 轮即可，不必 8）
- 若去掉 SoftSync 仍绿 → FEEDBACK 写明「本骨架 SoftSync 非必要」（削弱，不是失败）
- 墙钟 ≤30min

## 依赖
必须等 E01 FEEDBACK 关门且主控刷库/图后再发。
