# N02 — NPU：Encrypt 外形骨架不挂

| 字段 | 值 |
|------|-----|
| 状态 | **wait_npu** |
| DAG | `D-EXP-N02` → 逼近 `Q-ULT`（挂死维度） |
| 代码目录 | T09 目录 |
| 运营目录 | `…/tasks/N02-npu-encrypt-skel/` |
| 墙钟 | ≤ 40 min |

继承 COMMON。

## 目标

真机跑 T09 2-launch 骨架：**SynchronizeStream 返回**；优先不挂。  
正确性对拍有则记，无则 `PASS_SYNC_NPU` 即可本刀收口，IO 留后续刀。

## 前置

- T09 SIM `PASS_SYNC`  
- N01 PASS  
- `logs/npu-repro-draft.md` 已审  
- NPU 空闲

## 验收

- FEEDBACK + 完整 hang/trace 摘要  
- 失败必须沉 X*（主控刷 KB）

## 依赖

N01 + T09。
