# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（Encaps/Decaps **真 2-launch** 默认；Cloud SIM 全绿）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`main`**（改动在工作区；**未授权勿 commit/push**）。  
2. Cloud 首次：`bash scripts/clone-thirdparty.sh`（含 liboqs）。  
3. **今日 P0 真相**：Encaps + Decaps Phase-E 默认 **`prep_ntt` → `l18(ySrc=nullptr)`**（真 2 Host launch / 每 MIX 一轮 Cube）；Phase-D 默认 **`chain_ntt` → `chain_intt`**。  
4. 回退：`F203_ENCAPS_SPLIT_PREP` / `F203_DECAPS_SPLIT_PREP`（旧 3-launch）；旧双 Cube：`F203_*_FUSED*`。  
5. Cloud SIM：**Encaps PASS**、**Decaps full D+E PASS**（见 [`qa/2026-09/2026-09-03-…`](qa/2026-09/2026-09-03-Encaps-Decaps真2launch与GATE.md)）。  
6. 实机粘性：**未声称已消**；须单卡 reset + `FORCE_REBUILD` 后再加压。  
7. 关键修复：Encrypt prep∈MIX **可行**；同核 NTT 前须 **SoftSync + GATE(4/8)**（缺 GATE 曾 c 全错）。

### 刚关闭

| 项 | 说明 |
|----|------|
| **默认 3-launch** | 用户否决；已改为真 2-launch |
| **「Encrypt prep 不能进 MIX」误判** | 隔离证伪：`PREP_MIX_ONLY` PASS；根因是缺 GATE |
| **TRACE 107002** | host bug 已修（09-02） |

### 待办快照

| 项 | 说明 |
|----|------|
| **实机加压** | 搬 2-launch + TRACE 修 → FORCE_REBUILD → 单卡 reset → Encaps↔Decaps 多轮 |
| **T2-npu-env** | 按树分卡；见既有脚本 |
| **stable-512/768** | 须用户 `#交付#` |

---

## ★ 接手清单（2026-09-03）

| 优先 | 项 | 做什么 | 注意 |
|------|----|--------|------|
| **P0** | **实机验证 2-launch** | 搬码 → FORCE_REBUILD → reset → Encaps↔Decaps 加压 | 勿再默认 `FUSED_*` / `SPLIT_PREP` |
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
