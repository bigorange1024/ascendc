# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（实机：Encaps FORCE 后仍 3–5 轮粘死；拆双 Cube **未消粘**）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**（勿只拉 main；未授权勿合 main）。  
2. Cloud 首次：`bash scripts/clone-thirdparty.sh`（含 liboqs）。  
3. **今日 P0 真相**：Encaps/Decaps 默认 2-launch 已上分支；Cloud SIM 绿。  
4. **实机**：`FORCE_REBUILD` 后 Encaps 仍约 **3–5 轮**卡死；挂点两种——最后打印 **`prep_ntt` 或 `l18(ySrc=null)`** → 非仅第二段；**拆双 Cube 未消粘**。Decaps `K max=131`。详 qa 09-03 §6。  
5. 写 AscendC 前：Rule + engineering-notes。

### 刚关闭

| 项 | 说明 |
|----|------|
| **默认 3-launch** | 用户否决；已改为真 2-launch |
| **「Encrypt prep 不能进 MIX」误判** | 隔离证伪：`PREP_MIX_ONLY` PASS；根因是缺 GATE |
| **TRACE 107002** | host bug 已修（09-02） |

### 待办快照

| 项 | 说明 |
|----|------|
| **实机 Encaps 粘性** | FORCE 后仍 N 轮挂；对照 SPLIT_PREP / FUSED / KeyGen；`F203_L18_TRACE=1` |
| **实机 Decaps K≠0** | 正交 `F203_DECRYPT_FUSED` / `SPLIT_PREP` / 双 FUSED |
| **T2-npu-env** | 按树分卡；见既有脚本 |
| **stable-512/768** | 须用户 `#交付#` |

---

## ★ 接手清单（2026-09-03）

| 优先 | 项 | 做什么 | 注意 |
|------|----|--------|------|
| **P0** | **Encaps 粘性根因分诊** | TRACE + SPLIT_PREP vs 默认 vs FUSED vs KeyGen 多轮 | 已否定「只拆双 Cube 即够」 |
| **P0** | **Decaps K=131** | `F203_DECRYPT_FUSED` / `SPLIT_PREP` / 双 FUSED | 与粘性正交 |
| **P1** | alg19 错结果 | 干净卡复验 | **独立线** |
| **P2** | 512 审阅 | 与 NPU 线互不冲突 | |

**别做**：未授权 commit/push；从 frozen 抄码；`#交付#` stable-512/768（须点名）。

---

## ★ 当前真相（勿重复大工程）

| 项 | 状态 |
|----|------|
| **1024 Encaps/Decaps** | 默认真 **2-launch**；Cloud SIM 绿 |
| **768 / 512** | incubating **有条件完成**；禁 stable |
| **额度纪律** | Rule + Skill → Cloud-Agent额度文档 |

---

## ★ 下一刀

**P0**：借入机验证 2-launch 是否消粘性（对照 qa 09-02/09-03）。

可选：`bash scripts/npu_kem_one_trip.sh` · [`docs/engineering/实机一次搬码验收清单.md`](docs/engineering/实机一次搬码验收清单.md)。
