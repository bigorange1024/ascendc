# FROZEN — fix-f203-alg14-encrypt-2launch-k4（2026-06-30 关闭）

**判决日期**：2026-06-30  
**GitHub 来源**：家里 agent 提交 `7978f24` / `27cc93b`（2026-06-30）；办公室 fetch 后迁入本目录。

## 原角色

FIPS 203 Alg.14（ML-KEM-1024 PKE.Encrypt，k=4）**单 ACL session 重建探针**——家里 agent 按 KeyGen 蓝本**另建新目录**重搭 Encrypt 全链；声称 CPU+SIM `c.bin` 1568B max=0。

## 关闭原因

| # | 原因 | 说明 |
|---|------|------|
| 1 | **办公室未复验** | 家里 agent 在本地环境声称全链 PASS；**办公室未跑完**对该探针的 CPU+SIM 独立验收（含 stray dump 检查等；长链须用 `ENCRYPT_KERNEL_BUDGET_SEC`，非 15s 全局门禁） |
| 2 | **探针分叉** | 与既定活跃探针 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../../fix-f203-alg14-pke-encrypt-correctness-k4/) 并行维护整树副本，违反「在原探针迭代、G5 测通即冻结过渡 Gate」策略 |
| 3 | **继任已落地** | 办公室在原探针**原地**完成 at_r5 + MIX decode/pack + `run_g5_sim_full`；**G5 双模式 PASS**（c.bin max=0，SIM 无 507000，tick **922441**）— 见 [`../../fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md`](../../fix-f203-alg14-pke-encrypt-correctness-k4/STATUS.md) |
| 4 | **病根已沉淀** | R1（func_key≥5→507000）、R2（D2H 前缺 sync）结论已写入 [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)；不必保留第二份整树实现 |
| 5 | **验收口径不一致** | 本树 STATUS 要求 `ENCRYPT_KERNEL_BUDGET_SEC=1000` 等；仓库曾误将 15s 当作全仓默认（已废止，见 `docs/engineering/内核计算超时与性能定标.md`） |

## 合法继任（禁止从本目录抄码）

| 能力 | 路径 |
|------|------|
| **活跃 Encrypt 探针（G5）** | [`../../fix-f203-alg14-pke-encrypt-correctness-k4/`](../../fix-f203-alg14-pke-encrypt-correctness-k4/) |
| Gate 过渡冻结 | [`../../fix-f203-alg14-pke-encrypt-correctness-k4/frozen-gates/`](../../fix-f203-alg14-pke-encrypt-correctness-k4/frozen-gates/) |
| 旧 G3 四核 | [`../../fix-f203-alg14-pke-encrypt-correctness-k4/compute/frozen/`](../../fix-f203-alg14-pke-encrypt-correctness-k4/compute/frozen/) |
| funckey 原理 | [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md) |

## 讨论与证据

| 文档 | 内容 |
|------|------|
| [`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) | 办公室独立证伪 R1 + 原探针 at_r5 落地 |
| [`qa/2026-06/2026-06-30-Encrypt单session重建与SIM-funckey5-507000病根.md`](../../../qa/2026-06/2026-06-30-Encrypt单session重建与SIM-funckey5-507000病根.md) | 家里 agent 原始纪要（保留） |

## Agent 规则

- **可进入**阅读 `FROZEN.md`、`STATUS.md`（历史声称）、家里 commit 对照
- **禁止**复制/移植/fork 到活跃目录；**禁止**标为活跃基线或跑 CI 验收
