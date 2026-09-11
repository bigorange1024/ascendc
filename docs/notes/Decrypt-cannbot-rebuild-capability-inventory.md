# Decrypt / Decaps 重建 · 基础能力清单（FIPS 203 Alg.15 / Alg.21 · ML-KEM-1024）

> **用途**：cannbot 直调重拼 **Decrypt → Decaps** 前的积木表。  
> **参数**：\(n=256\)，\(q=3329\)，\(k=4\)，\(d_u=11\)，\(d_v=5\)。  
> **目标（锁定）**：  
> 1. **正确**：`m`/`K` 与 **liboqs** 权威交叉一致（禁 python 冒充）。  
> 2. **反卡死**：同路径反复 `-r npu`（建议 ×30）墙钟内返回；`KERNEL_COMPUTE_BUDGET_SEC` 默认 **180**；超时=有问题。  
> **禁抄**：完整 PKE Decrypt / KEM Decaps 算子树，以及临时 `RB-T25/26/27` **源码当模板**（STATUS/FEEDBACK 判决式教训可参考）。  
> **可参考**：`docs/notes` 定稿、独立基础探针 STATUS、`library/shared`、`graph-tests/{toys,bricks}`、活跃 Encaps 路径 **思路**（T22–T24）、NTT+编解码准链探针（契约可参考，**禁止大段照抄**）。  
> **配套**：[`Decrypt-cannbot-rebuild-work-mode.md`](Decrypt-cannbot-rebuild-work-mode.md) · [`Decrypt-cannbot-rebuild-kb.md`](Decrypt-cannbot-rebuild-kb.md) · DAG [`docs/rg-decrypt-cannbot-rebuild.yaml`](../rg-decrypt-cannbot-rebuild.yaml) · 反卡死 [`MIX-Encrypt-Encaps-反卡死拓扑技术总结.md`](MIX-Encrypt-Encaps-反卡死拓扑技术总结.md)。

---

## 0. 战役目标一句话

| 算子 | 输入 → 输出 | 关闭条件 |
|------|-------------|----------|
| **Decrypt**（Alg.15） | `dk_pke`(1536)+`c`(1568) → `m`(32) | CPU+NPU `m≡liboqs`；×N 不挂 |
| **Decaps**（Alg.21） | `dk_kem`(3168)+`c`(1568) → `K`(32) | CPU+NPU `K≡liboqs`；合法/拒绝路径；×N 不挂 |
| **关闸** | 设备 Encaps↔设备 Decaps | 同路径往返 `K` 一致 + 压测不挂（`Q-RT-HANG`） |

**非目标（本阶段）**：`examples/stable-*` 晋级；修旧 alg15/21；在 T25–T27 上继续堆功能。

---

## 1. Alg.15 Decrypt 步骤 ↔ 能力块

| FIPS Alg.15（摘要） | 能力块 | 仓内候选（活跃 / 积木） | 证据级别 | 用法 |
|---------------------|--------|--------------------------|----------|------|
| 行：\(c→(u,v)\) Decompress+ByteDecode | Decompress₁₁/₅ + ByteDecode_d | `pass-f203-compress-d-vec-k4` / `pass-f203-*-d-vec-k4`；`library/shared/f203_byte_codec`；战役砖 `RB-T05`（BD₁₂ 思路） | CPU+SIM/NPU（分 d） | **可复用积木**；Decrypt unpack 壳须新建 |
| 行：\(dk→ŝ\) ByteDecode₁₂ | ByteDecode₁₂ | `RB-T05-bytedecode12`；shared codec | 战役内 NPU | **可复用积木** |
| \(û←\)NTT\((u)\) | 正向 NTT poly-batch | `pass-fix-…-vec-k4-v2`；toys `RB-T01/T03`；`RB-T12` NTT(y) **思路** | CPU+SIM/NPU | **契约+积木可参考**；输入是 **u** 非 y，禁抄 Encrypt 核 |
| \(ŵ←Σ_j\) MultiplyNTTs\((ŝ_j,û_j)\) | 内积 / Alg.11–12 | `pass-fix-f203-alg11-12-*`；innerproduct 探针；定稿 `F203-innerproduct-k4-技术总结.md` | CPU+SIM | **积木可复用**；Decrypt 拼装壳新建 |
| \(w←\)INTT\((ŵ)\) | INTT | polyvec8 INTT；toys 有界 NTT↔INTT | CPU+SIM | **可复用积木** |
| \(m←\) 与 \(v\) 的 Compress₁ 逆 / extract | Compress₁ 路径 + 写出 | Compress 探针；Encaps pack **DataCopy 写出模式**（X12） | 分项有 | **缺口：独立 Decrypt extract 验收** |
| 全程 MIX | CrossCore 1/3(+4) | toys T01–T03；Encaps T22 反卡死检查单 | NPU | **模式可复用**；禁抄 alg15 FSM / SoftSync |

---

## 2. Alg.21 Decaps 步骤 ↔ 能力块

| FIPS Alg.18/21（摘要） | 能力块 | 候选 | 用法 |
|------------------------|--------|------|------|
| \(m'←\)Decrypt | 上表全链 | 本战役 Decrypt 新目录 | **依赖 Decrypt 收口** |
| \((K',r')←G(m'\|h)\)；\(h\) 自 dk 切片 | 设备 SHA3-512 | Encaps T22 Launch1 **G 思路**；`library/shared` SHA3 | **可参考思路**；禁抄 Encaps/Decaps 核 |
| \(c'←\)Encrypt\((ek,m';r')\) | 已收口 Encaps/Encrypt 拓扑 | T22–T24 **LAYOUT/反卡死**；禁抄源码 | Re-Encrypt **新目录重拼**，接 Encaps 积木契约 |
| FO：\(c{?}{=}c'\)；选 \(K'\) 或 \(J(z\|c)\) | 比对 + SHAKE256 | 无独立绿砖（临时 T26 教训可读） | **缺口：设备 FO 砖** |
| I/O | `dk_kem` 3168 布局 | Alg.19 note / Encaps inventory | 布局契约只读 |

---

## 3. 缺口（战役内状态 · 2026-09-09）

| ID | 缺口 | 状态 |
|----|------|------|
| **DG1** | Decrypt prep 壳 | **closed**（D01；NPU 回填） |
| **DG2** | NTT(u)+su_dot MIX | **closed**（D02） |
| **DG3** | INTT+extract→m | **closed**（D03） |
| **DG4** | 设备 G | **closed**（K01） |
| **DG5** | Re-Encrypt | **closed**（K02） |
| **DG6** | 设备 FO | **closed**（K03；×30） |
| **DG7** | Encaps↔Decaps 往返 | **closed**（K04；≈100 收口） |

**战役外**：PKE/KEM KeyGen → **新战役已启动**（2026-09-09）：[`KeyGen-cannbot-rebuild-kb.md`](KeyGen-cannbot-rebuild-kb.md) · DAG `docs/rg-keygen-cannbot-rebuild.yaml` · ops `graph-tests/keygen-rebuild-ops/`。原 `Q-KEYGEN-HANG-PROOF` 由新门禁 `Q-PKE-KG` / `Q-KEM-KG` / `Q-KG-HANG` 承接。

新建约束仍成立：**不得**从 Decrypt/Decaps 旧树搬码。定稿原理见 [`MIX-Decrypt-Decaps-反卡死拓扑技术总结.md`](MIX-Decrypt-Decaps-反卡死拓扑技术总结.md)。  
为何重建不挂：KB §B2.1 / DAG `F-WHY-NO-HANG`。

---

## 4. 禁抄索引（仅登记「曾绿」，不作模板）

- `pass-fix-f203-alg15-*`、`*-alg15-pke-decrypt-*`
- `pass-fix-f203-alg21-*`、`*-kem-decaps-*`（含 `-ct`）
- `examples/**/*decrypt*`、`*kem-decaps*`
- `graph-tests/enc_related/RB-T25|T26|T27-*/**` **实现源码**（FEEDBACK/STATUS/判决 OK）
- `**/frozen/**` 内 Decrypt/Decaps 源码
- 任意 `f203_decrypt_*` / `f203_kem_dec_*` / `l18_l19` **实现文件**

笔记中绑 alg15/21 案例的段落：**只读不变量**，禁止对照源码抄写。

---

## 5. 合法参考（准链 / 积木）

| 类型 | 路径 | 允许 |
|------|------|------|
| 握手玩具 | `graph-tests/toys/RB-T01…T03` | 读 LAYOUT / 同步模式；可小改实验，勿整树 fork 当 Decrypt |
| 编解码砖 | `graph-tests/bricks/RB-T04…T06` | BD₁₂ / pack **契约**；Decrypt unpack 自写 |
| Encaps 收口 | `RB-T22…T24` STATUS/FEEDBACK/LAYOUT | 双 launch、flag、G、liboqs 交叉 **思路** |
| NTT/CBD/内积探针 | `ascendc-tests/**/pass-fix-f203-2s1e*`、`alg7`、`alg8`、`alg11*`、`pass-f203-*-d-vec-k4` | 契约 + 小段 API 用法；**禁大段照抄进 Decrypt 核** |
| shared | `library/shared/**` | 头与原语 |
| cannbot | `thirdparty/cannbot-skills/ops/**` | sync-audit / crash-debug / api-crosscore |

---

## 6. 定稿笔记（契约）

| 主题 | 路径 |
|------|------|
| Decrypt 切分原理 | `F203-Alg15-Decrypt-2launch编排技术总结.md`（原理；**禁抄生产 1-kernel 源码**） |
| 反卡死 | `MIX-Encrypt-Encaps-反卡死拓扑技术总结.md` |
| NTT / Gather | `MLKEM-NTT-实现总结.md` · `MLKEM-NTT-向量与标量实现指南.md` |
| 内积 | `F203-innerproduct-k4-技术总结.md` |
| Compress/Decompress / BE | `F203-Compress-Decompress-向量实现指南.md` · `F203-ByteEncode-ByteDecode-d-向量与标量选型.md` |
| Encaps 已收口 KB | `Encrypt-cannbot-rebuild-kb.md`（只读继承反卡死/X*） |
| 旧 Decaps SIM 污染 | `F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`（失败史；非实现模板） |

---

**状态**：2026-09-09 — DG1–DG7 **战役内已关**（D01–D07 / K01–K04）；补砖级 NPU×30 进行中。定稿见 [`MIX-Decrypt-Decaps-反卡死拓扑技术总结.md`](MIX-Decrypt-Decaps-反卡死拓扑技术总结.md)。
