# Encrypt 重建 · 基础能力清单（FIPS 203 Alg.14 / ML-KEM-1024）

> **用途**：cannbot 直调重拼 Encrypt 前的积木表。  
> **参数**：\(n=256\)，\(q=3329\)，\(k=4\)，\(\eta_1=\eta_2=2\)，\(d_u=11\)，\(d_v=5\)。  
> **禁抄**：完整 PKE Encrypt / KEM Encaps / Decaps 算子树的 kernel 与 host 编排（表末索引）。  
> **可参考契约**：`docs/notes` 定稿、独立探针 STATUS、`library/shared` 头。  
> **配套**：工作模式 · KB · DAG 见同前缀文件。

---

## 1. Alg.14 步骤 ↔ 能力块

| FIPS Alg.14（摘要） | 能力块 | 仓内候选（活跃） | 证据级别 | 用法 |
|---------------------|--------|------------------|----------|------|
| 行 2：\(t̂←\) ByteDecode₁₂(ek) | ByteDecode₁₂ | `library/shared/f203_byte_codec/byte_decode12_*.hpp`；d∈{4,5,10,11} 探针 `pass-f203-alg6-bytedecode-d-vec-k4` | Decode₁₂ **无独立终态探针**；d≠12 有 CPU+SIM | 积木/头可用；Decode₁₂ 须新建独立验收 |
| 行 3–7：Â←SampleNTT(ρ‖…) | SHAKE128 + Alg.7 | `shake_xof_kernel/`；`pass-fix-f203-alg7-sample-ntt-k4`；`pass-fix-f203-alg13-lines3-7-a-hat-k4` | CPU+SIM | **可复用积木** |
| 行 8–15：y/e₁/e₂←CBD(PRF) | SHAKE + Alg.8 η=2 | `pass-fix-f203-alg8-cbd-eta2-k4`；`fips203_se_sample/`；`pass-fix-f203-alg13-lines8-15-se-k4` | CPU+SIM | **可复用积木**（I/O 是 coins 而非 KeyGen SEED） |
| 行 16：ŷ←NTT(y) | 正向 NTT（poly-batch） | `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`；`pass-fix-f203-stage123-ntt-intt-polyvec8-vec` | CPU+SIM（基线 tick 见 INDEX） | **可复用积木 + 数学契约**（S1–S3 禁 Gather） |
| 行 17：u←INTT(Âᵀ∘ŷ)+e₁ | 内积/MultiplyNTTs + INTT + mod q | `pass-fix-f203-alg11-12-*`；`pass-fix-f203-*-innerproduct-*`；polyvec8 INTT；`f203_mod_q/` | CPU+SIM | **积木可复用**；Âᵀ∘ŷ 拼装属编排缺口 |
| 行 18–19：v←INTT(⟨t̂,ŷ⟩)+e₂+μ | 内积 + INTT + μ_embed + mod q | 同上；μ 无独立 shared | 内积/INTT 有；μ **无独立探针** | μ←m：**缺口，须新建** |
| 行 20–22：Compress + ByteEncode → c | Compress_{11/5} + BE_d | `pass-f203-compress-d-vec-k4`；`pass-f203-byteencode-d-vec-k4`（含 d=5/11） | CPU+SIM | **可复用积木**；c₁‖c₂ 布局壳为缺口 |
| 全程 MIX | CrossCore SET/WAIT | `pass-toy-mix-s123-byteencode-k2`；NTT 探针 MIX | CPU+SIM | **可复用模式**；禁从 alg14 抄 FSM |
| Host | tiling / LUT / session | `shake_xof_kernel/tiling_host.hpp`；LUT 惯例；`acl_session/`；`fips203_host_rng/` | KeyGen/toy 间接 | 壳可复用；Encrypt 全 host **禁抄** |

---

## 2. 缺口（必须新建，不得从 Encrypt 树搬码）

| ID | 缺口 | 建议验收形态 |
|----|------|----------------|
| G1 | **μ_embed / Decompress₁(m→256)** | **已关**：`RB-T04-mu-embed` SIM+NPU |
| G2 | **ByteDecode₁₂(ek→t̂)** 独立终态 | **已关**：`RB-T05-bytedecode12` SIM+NPU |
| G3 | **Encrypt 域拼装**：Âᵀ∘ŷ ‖ ⟨t̂,ŷ⟩ → 双路 INTT → +e/+μ | **已关**：Host T08；设备 T10/T11；Encaps 全链 T22–T24 NPU（含×30 不挂） |
| G4 | **prep 壳**：ρ←ek 尾；coins→(y,e₁,e₂) | **已关**：Host T07；设备 G/coins 随 T22 Launch1 |
| G5 | **密文 pack 壳**：Compress 后 u/v → c（1568B） | **已关**：`RB-T06-cipher-pack`；Encaps 链 T22–T24 |

---

## 3. 禁抄索引（仅登记「曾绿」，不作模板）

- `pass-fix-f203-alg14-*`、`*-alg14-pke-encrypt-device-k4`
- `pass-fix-f203-alg20-*` / `alg21-*`（含 `-ct`）
- `examples/**/stable|exp-*encrypt*`、`*kem-encaps*`、`*kem-decaps*`
- 上述树内任何 `f203_encrypt_*` / `f203_mu_embed` / `l18_l19` **实现文件**

笔记中绑 alg14 案例的段落（如 UB 驻留、compute-tail）：**只读不变量**，禁止对照源码抄写。

---

## 4. 定稿笔记（契约）

| 主题 | 路径 |
|------|------|
| NTT 数学 / poly-batch | `docs/notes/MLKEM-NTT-实现总结.md` |
| NTT 设备 / Gather 范围 | `docs/notes/MLKEM-NTT-向量与标量实现指南.md` |
| SampleNTT | `docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md` |
| CBD η=2 | `docs/notes/F203-CBD-eta2-性能优化技术总结.md` |
| 内积 | `docs/notes/F203-innerproduct-k4-技术总结.md` |
| Compress / Encode | `docs/notes/F203-Compress-Decompress-向量实现指南.md`、`F203-ByteEncode-ByteDecode-d-向量与标量选型.md` |
| Pipe / TQue | `docs/notes/ascendc-TQue与Pipe框架知识库.md` |
| SIM session | `docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`（平台约束；案例附录勿抄） |

---

**状态**：G1–G5 **战役内已关**；Encaps 设备路径以 T22–T24 为准。设备 Decaps 不在本清单范围。
