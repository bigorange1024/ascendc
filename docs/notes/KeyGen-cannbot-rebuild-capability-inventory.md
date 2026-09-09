# KeyGen 重建 · 基础能力清单（FIPS 203 Alg.13 / Alg.19 · ML-KEM-1024）

> **用途**：cannbot 直调重拼 **PKE KeyGen → KEM KeyGen** 前的积木表。  
> **参数**：\(n=256\)，\(q=3329\)，\(k=4\)（ml_kem_1024）。  
> **目标（锁定）**：  
> 1. **正确**：`ek_pke`/`dk_pke`、`ek_kem`/`dk_kem` 与 **liboqs** 字节一致（禁 python 冒充权威）。  
> 2. **反卡死**：同路径反复 `-r npu`（中间×30 / 算子级×100）；`KERNEL_COMPUTE_BUDGET_SEC` 默认 **180**；超时=缺陷。  
> **禁抄（算子级）**：`examples/**/*keygen*`、`pass-fix-f203-alg13*keygen*`、`pass-fix-f203-alg19*keygen*`、任意 `f203_keygen_*` / `mmad_custom` KeyGen 树、`**/frozen/**` KeyGen 源码。  
> **可参考**：`docs/notes` 定稿与 **算法契约**；独立分项探针 STATUS（SampleNTT/CBD/NTT/内积/ByteEncode）；`library/shared`；`graph-tests/{toys,bricks}`；Encaps/Decrypt 重建 **LAYOUT/反卡死思路**（禁抄其核当 KeyGen 模板）。  
> **配套**：[`KeyGen-cannbot-rebuild-work-mode.md`](KeyGen-cannbot-rebuild-work-mode.md) · 工程 KB [`ascendc-engineering-kb.md`](ascendc-engineering-kb.md) · DAG [`docs/rg-ascendc-engineering.yaml`](../rg-ascendc-engineering.yaml) · 反卡死 notes。

---

## 0. 战役目标一句话

| 算子 | 输入 → 输出 | 关闭条件 |
|------|-------------|----------|
| **PKE KeyGen**（Alg.13） | `seed_d`（约定）→ `ek_pke`(1568)+`dk_pke`(1536) | CPU+NPU ≡liboqs；×N 不挂 |
| **KEM KeyGen**（Alg.19 / Alg.16） | 同 seed 约定 → `ek`(1568)+`dk_kem`(3168) | CPU+NPU ≡liboqs；×N 不挂 |
| **关闸** | 干净卡反复 npu | 运营 QUEUE done；图上薄指针 `J-REBUILD-TOPO-PASSES-GATES`（≠ stable 晋级） |

**非目标（本阶段）**：`examples/stable-*` 晋级；修旧 alg13/19 探针；在 stable KeyGen 上打补丁冒充重建。

---

## 1. Alg.13 PKE KeyGen 步骤 ↔ 能力块

| FIPS Alg.13（摘要） | 能力块 | 仓内候选（活跃 / 积木） | 证据级别 | 用法 |
|---------------------|--------|--------------------------|----------|------|
| \(d\) 派生 / \(G(d\|k)→(ρ,σ)\) | SHA3-512 / Derand | Encaps T22 G 思路；`library/shared` SHA3；KeyGen note 契约 | 分项/战役 | **新壳**；禁抄 KeyGen Derand 算子树 |
| 行 3–7：Â←SampleNTT(ρ‖…) | SHAKE128 + Alg.7 | `pass-fix-f203-alg7-*`；`alg13-lines3-7-a-hat`；`shake_xof_kernel` | CPU+SIM | **积木可复用**；KeyGen 拼装壳新建 |
| 行 8–15：ŝ/ê←CBD(PRF(σ)) | SHAKE + Alg.8 η₁=2 | `pass-fix-f203-alg8-cbd-*`；`alg13-lines8-15-se` | CPU+SIM | **积木可复用**；禁抄 KeyGen prep 整核 |
| 行 16：NTT(ŝ)、NTT(ê) | 正向 NTT poly-batch | `pass-fix-…-vec-k4-v2`；toys NTT | CPU+SIM/NPU | **契约+积木**；S1–S3 禁 Gather/limbsplit |
| 行 17：t̂←Â∘ŝ+ê | hat 点积 / MultiplyNTTs + add | `alg11-12`；innerproduct；定稿 innerproduct note | CPU+SIM | **积木可复用**；编排壳新建 |
| 行 18–20：ByteEncode₁₂(t̂/ŝ) | BE₁₂ | bricks `RB-T*`；`f203_byte_codec`；Encrypt pack 写出模式（X12） | 战役/探针 | **积木**；写出须 DataCopy |
| 行 21：ek=BE(t̂)‖ρ；dk=BE(ŝ) | 拼接 | 布局契约见 Alg.19 note | 文档 | **壳新建** |

---

## 2. Alg.19 KEM KeyGen 增量 ↔ 能力块

| FIPS Alg.16/19（摘要） | 能力块 | 候选 | 用法 |
|------------------------|--------|------|------|
| \((ek,dk_pke)←\)PKE.KeyGen | 上表全链 | 本战役 PKE 新目录 | **依赖 PKE 收口** |
| \(H(ek)\)；采 \(z\)；拼 `dk_kem` | SHA3-256 + 布局 | Encaps/Decrypt FO 哈希思路；Alg.19 note §1.4 | **新 launch 或尾段**；禁抄 alg19 算子树 |
| I/O | `dk_kem` 3168 = `dk_pke‖ek‖H(ek)‖z` | Alg.19 note | 布局契约只读；权威=liboqs |

---

## 3. 缺口（战役启动 · 2026-09-09）

| ID | 缺口 | 状态 |
|----|------|------|
| **KG1** | seed→(ρ,σ) + SampleNTTÂ 壳 | **closed**（P01 PASS_CPU） |
| **KG2** | PRF+CBD → ŝ/ê | **closed**（P01 同刀） |
| **KG3** | NTT(ŝ/ê) MIX | **closed**（P02 PASS_CPU） |
| **KG4** | Â∘ŝ+ê + ByteEncode₁₂ + ek‖ρ | **closed**（P03 PASS_CPU） |
| **KG5** | PKE 全链拼装 + liboqs | **closed**（P04 CPU+NPU×30） |
| **KG6** | KEM：H(ek)+z + dk_kem 拼装 | **closed**（K01 CPU+SIM+NPU×30） |
| **KG7** | KEM 全链 + 反卡死压测 | **closed**（K02 CPU+NPU×30 ≡liboqs_kem） |

拓扑定稿见工程 KB §2 / §7.4；禁默认沿用旧「prep AHAT=2 + 单长 MIX」而不论证。

---

## 4. 禁抄索引（算子级 · 曾绿不作模板）

- `examples/stable|incubating/**/*keygen*`
- `pass-fix-f203-alg13-device-keygen-*`、`pass-fix-f203-alg19-kem-keygen-*`
- 任意目录内 `f203_keygen_prep*`、`f203_keygen_ek*`、KeyGen 专用 `mmad_custom.cpp` 整树
- `**/frozen/**` 内 KeyGen 源码
- Encrypt/Decrypt 重建核 **整文件 fork 改名当 KeyGen**（可复用 **契约与同步模式**，禁整核抄）

笔记（prep 双 AIV、Pipe、Alg.19）：**只读不变量与失败史**，禁止对照算子源码抄写。

---

## 5. 合法参考（准链 / 积木）

| 类型 | 路径 | 允许 |
|------|------|------|
| 握手玩具 | `graph-tests/toys/RB-T01…` | LAYOUT / 短 CrossCore |
| 编解码砖 | `graph-tests/bricks/**` | BE/BD 契约 |
| Encaps/Decrypt 收口 | `enc_related/RB-T22…`、`dec_related/RB-D*` FEEDBACK | 多 launch、flag、DataCopy、liboqs **思路** |
| 分项探针 | alg7 / alg8 / 2s1e NTT / alg11-12 / byteencode | 契约 + 小段 API；**禁大段照抄进 KeyGen 核** |
| shared | `library/shared/**` | 头与原语 |
| cannbot | `thirdparty/cannbot-skills/ops/**` | sync-audit / crosscore |

---

## 6. 定稿笔记（契约）

| 主题 | 路径 |
|------|------|
| KEM I/O / Alg.16–19 | `F203-KEM-Alg19-KeyGen设备全链技术总结.md`（契约；禁抄实现） |
| prep 半写失败史 | `F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`（**反例**） |
| prep Pipe | `F203-KeyGen-prep-Pipe细同步技术总结.md` |
| 反卡死 | `MIX-Encrypt-Encaps-反卡死拓扑技术总结.md` §4.3–5 |
| Decrypt 风险衔接 | `Decrypt-cannbot-rebuild-kb.md` §B2.2 |
| NTT / 内积 / BE | MLKEM-NTT notes · innerproduct · ByteEncode notes |

---

**状态**：2026-09-09 — incubating 关闸（CPU+NPU×30）；实现落 `graph-tests/kg_related/RB-K*`。
