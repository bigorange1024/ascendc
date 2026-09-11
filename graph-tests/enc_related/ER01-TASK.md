# ER01 — enc_related 首刀任务书

> 图谱节点：`D-EXP-ER01`  
> 目录（待建）：`graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/`  
> 主控派 subagent 编码；本文件定目标与验收。

## 目标

做出 **Encrypt 外形** 的双段 launch 骨架（不是抄旧 Encrypt）：

1. Launch1：轻量 prep 桩（可不含真 SHAKE；可 Host 喂表）
2. Launch2：MIX 计算壳 = **生产 GATE 时序**（AIC 先 WAIT(4)）+ **有界真积木体量**（可复用 T06 级 Vec MAC 或更近内积；禁止假空转加码当唯一加压）+ NTT/INTT 握手用 **1/3**（禁 5/7）

核心验收：**SIM 不挂**（及 CPU）。**不对** liboqs / 密文正确性。

## 必用 cannbot（主动）

| 时机 | Skill / 脚本 |
|------|----------------|
| 写 CrossCore 前 | 读 `ops/ascendc-api-best-practices/references/api-crosscore-sync.md` |
| 编码完成后 | `ops/ascendc-sync-audit/scripts/sync_audit.py` 扫本刀全部 `.cpp/.hpp`，红线原样写入 STATUS |
| 若 SIM 挂 | 按 `ops/ascendc-sync-audit/workflows/deadlock-triage.md` + `ops/ascendc-crash-debug` 分诊 |

## 永禁（KB）

5/7；Wait 中 SyncAll；自造 SoftSync；抄旧 Encrypt 核；把 CPU 绿当卡死已解。

## 墙钟

单刀编码+CPU+SIM ≤ 40min；超时 ABORT 回报，不傻等。

## 交付

- 目录内 `STATUS.md`（含 sync_audit 摘要）  
- CPU + `SIM_DIRECT=1` sim 日志要点  
- 回写主控：通 / 挂 / 超时 + 学到的一条
