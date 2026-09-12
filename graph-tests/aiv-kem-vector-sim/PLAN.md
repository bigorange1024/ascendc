# PLAN · 单 AIV 全向量 KEM（SIM-only）

## 0. 硬约束

见 KB L1–L7 / A1–A5 / B1–B5。运行：`bash run.sh -r cpu` 与 `SIM_DIRECT=1 bash run.sh -r sim`；禁 `-r npu`。

## 1. 波次

| Wave | 内容 | 出口 |
|------|------|------|
| W0 | KB+清单+图谱+QUEUE | rg_validate OK |
| W1 | 向量积木：polyvec NTT/INTT、采样/哈希、matvec、pack | 各刀 cpu+SIM 对拍 |
| W2 | **AE-E**：单 AIV 单 launch Encrypt ≡ liboqs | c max=0 |
| W3 | **AE-P**：单 AIV 单 launch Encaps ≡ liboqs | c/K max=0 |

## 2. 拼装策略

优先：**一核一 launch 内串联向量积木**（UB/GM 流水）。采样/SHA3 可先独立探针再打进同 launch；最终 Encrypt/Encaps 入口必须单 launch。

## 3. cannbot-skills

每刀：`ascendc-sync-audit`；设计参考 `ascendc-tiling-design` / `ascendc-api-best-practices`；壳参考 `ascendc-direct-invoke-template`。
