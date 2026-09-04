# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-04（用户锁定：**优先 PKE Encrypt/Decrypt 卡死**，不是 KEM）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. **主线卡死 = PKE**：`stable-…-pke-encrypt-k4` + `stable-…-pke-decrypt-k4`。  
3. **KEM Encaps/Decaps 非本轮优先**（可共享 l18/握手教训，但上机顺序勿把 KEM 抬到 PKE 前）。  
4. Decrypt hang 图谱/toy 已 T0–T4；Encrypt hang 图历史多采自 Encaps，**实机验收须对 PKE Encrypt 加压**。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | 用户 NPU：**先 PKE Encrypt，再 PKE Decrypt**（同卡 1；各 FORCE 一次后连跑） |
| **P1** | 收日志刷新图谱；可选 toy 对照 |
| **P2** | KEM Encaps Hostμ / Decaps K=131 — **不挡 PKE** |

---

## ★ 图谱与用例对应（勿混）

| 卡死线 | 实机用例 | 图谱 |
|--------|----------|------|
| PKE Encrypt | `examples/stable/.../stable-fips203-mlkem-pke-encrypt-k4` | `rg-kem-encrypt-hang.yaml`（机制可借；验收以 **PKE** 为准） |
| PKE Decrypt | `examples/stable/.../stable-fips203-mlkem-pke-decrypt-k4` | `rg-kem-decrypt-hang.yaml` |
| KEM（次优先） | kem-encaps / kem-decaps | Encaps hang 历史样本 / K=131 另图 |

Decrypt SIM 沉积：合法绿；缺 SET(4)/仅 R2 缺 SET(4)→124；空/脏 SoftSync 非 SIM hang。
