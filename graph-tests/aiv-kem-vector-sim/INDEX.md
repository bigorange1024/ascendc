# aiv-kem-vector-sim — 单 AIV · 全向量 · Encrypt/Encaps（SIM）

> **状态（2026-09-12）**：**完成（SIM 全 AscendC）**  
> - **AE-E2 PASS**：纯 `ek|m|coins`；DEVICE CBD+SampleNTT+ByteDecode+Encrypt；`c`≡liboqs（SIM tick≈**1885458**，主控复验）  
> - **AE-P2 PASS**：纯 `ek|m`；DEVICE FO+CBD+SampleNTT+ByteDecode+Encrypt；`c/K`≡liboqs（SIM tick≈**1977711**，主控复验）  
> - 未上 NPU；文档沉淀后推送 sticky 分支（用户授权）

| 文档 | 路径 |
|------|------|
| 资产清单 | [ASSETS.md](ASSETS.md) |
| PLAN / QUEUE | [PLAN.md](PLAN.md) · [QUEUE.md](QUEUE.md) |
| 任务书 | [tasks/TASK-AE-FULL-ASCENDC.md](tasks/TASK-AE-FULL-ASCENDC.md) |
| KB | [Encrypt-aiv-vector-kb.md](../../docs/notes/Encrypt-aiv-vector-kb.md) |
| 图谱 | [rg-encrypt-aiv-vector.yaml](../../docs/rg-encrypt-aiv-vector.yaml) |
| Encrypt | [AE-E-encrypt/STATUS.md](AE-E-encrypt/STATUS.md) |
| Encaps | [AE-P-encaps/STATUS.md](AE-P-encaps/STATUS.md) |

与 `enc-encaps-cann-ntt-sim`（Cube/多 launch）**正交**。


**架构收益**：单 AIV 串多例无需组件同步；N 个 AIV → N 路用例并行。
