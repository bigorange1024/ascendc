# ML-KEM-512 P1：缺项对照（补缺图）与必建用例表（已定稿）

**状态**：**P1 定稿**（2026-07-27）· 实现未开（P0 目录壳已落）  
**依据**：[参数卡](fips203-mlkem512-parameter-card.md) · [完整计划](../research/MLKEM-512-从0到exp完整实现计划.md)  
**下一阶段**：P2 W0 起按表建探针；本文件不授权跳过 customspec（`examples/`）

### 用语（勿另造词）

以教材为准：**缺项** = 闭包中尚未获证的先决；**补缺** = 把缺项变成已获证的过程（旧称「补洞」，已废）；本文「补缺图」= 缺项与补齐策略对照表。  
**禁止**另立「洞 / 新洞 / 洞级 / 补洞」等说法。

---

## 0. P 与 W 编号约定

与 [fips203-mlkem768-p1-gap-and-cases.md](fips203-mlkem768-p1-gap-and-cases.md) §0 同构：

| 符号 | 含义 |
|------|------|
| **P0–P3** | 方法论主阶段 |
| **W0–W4** | **P2/P3 内**工程子波次 |
| **glue** | P3 收尾：registry + RT + liboqs-512 KAT |

| P | W | 内容 |
|---|---|------|
| P0 | — | 参数卡、CT、目录壳、registry 骨架、sizes |
| P1 | — | 缺项对照（补缺图）、必建用例表定稿 |
| P2 | W0 | 编码/压缩 + CBD η=2/η=3 |
| P2 | W1 | SampleNTT 2×2 + NTT/INTT polyvec4 + Multiply/Inner |
| P2 | W2 | PKE device D13–D15 |
| P2 | W3 | KEM device D19–D21ct |
| P3 | W4 | incubating E13–E21ct |
| P3 | glue | registry + AscendC RT + **liboqs-512 KAT** |

用例表 **P、W 分列**；禁止 `P2-W0` 合并格。

---

## 1. 相对 ML-KEM-768 的缺项对照（补缺图）

| 能力块 | 768 | 512 策略（补缺） | 备注（高/中/低，非正式） |
|--------|-----|------------------|------|
| SHAKE / Keccak | shared | **继承**；改次数/长度 | 低 |
| CBD \(\eta=2\) | 有 | Encrypt 等仍要；**轻量重挂 k=2 packing** | 中 |
| CBD \(\eta=3\) | **无**（缺项） | **新建** Alg.8 η=3 探针（KeyGen 主路径） | **高** |
| Compress 10/4 | 有模式 | 512 树重挂 + 对拍 | 中 |
| ByteEncode/Decode | 有 | 主路径 10/4 + 密钥域 12 | 中 |
| SampleNTT | \(3\times3\) | **\(2\times2\)** | 中高 |
| NTT / 内积几何 | polyvec6 | **单 cube + polyvec4** | **高** |
| PKE/KEM device+exp | 全套 | **同构重建** I/O 800/1632/768 | **高** |
| liboqs 交叉 | 可选 | **本阶段必达**（KEM helper 已可切 512） | **高（工程）** |
| frozen poly2 等 | — | **只读判决，不抄码** | — |
| stable | 不做 | **本阶段不建** | — |

---

## 2. 必建用例总表（已锁）

### 2.1 探针 — `ascendc-tests/ml-kem/ml-kem-512/`

| ID | P | W | 目录 | 作用 | 最小验收 |
|----|---|---|------|------|----------|
| **B1** | P2 | W0 | `pass-fix-f203-compress-decompress-du10-dv4-k2` | Compress/Decompress \(d_u=10,d_v=4\) | CPU+SIM |
| **B2** | P2 | W0 | `pass-fix-f203-byteencode-decode-d-k2` | ByteEncode/Decode：至少 d=10,4,12 | CPU+SIM |
| **B3a** | P2 | W0 | `pass-fix-f203-alg8-cbd-eta2-k2` | CBD \(\eta=2\) × k 维打包 | CPU+SIM |
| **B3b** | P2 | W0 | `pass-fix-f203-alg8-cbd-eta3-k2` | **CBD \(\eta=3\)**（相对 768 的缺项） | CPU+SIM；对拍 shared/liboqs |
| **B4** | P2 | W1 | `pass-fix-f203-alg7-sample-ntt-k2` | SampleNTT；\(i,j\in\{0,1\}\) | CPU+SIM |
| **B5** | P2 | W1 | `pass-fix-f203-stage123-ntt-intt-polyvec4-k2` | Stage1–3；**真 polyvec4 + S-1** | CPU+SIM；禁 S1–S3 Gather |
| **B6** | P2 | W1 | `pass-fix-f203-alg11-12-multiply-inner-k2` | MultiplyNTTs + Inner（\(k=2\)） | CPU+SIM |
| **D13** | P2 | W2 | `pass-fix-f203-alg13-device-keygen-k2` | Alg.13 → ek/dk_pke | CPU+SIM |
| **D14** | P2 | W2 | `pass-fix-f203-alg14-pke-encrypt-device-k2` | Alg.14 → c(768) | CPU+SIM |
| **D15** | P2 | W2 | `pass-fix-f203-alg15-pke-decrypt-device-k2` | Alg.15 → m | CPU+SIM；与 D14 RT |
| **D19** | P2 | W3 | `pass-fix-f203-alg19-kem-keygen-device-k2` | Alg.19 | CPU+SIM |
| **D20** | P2 | W3 | `pass-fix-f203-alg20-kem-encaps-device-k2` | Alg.20 | CPU+SIM |
| **D21** | P2 | W3 | `pass-fix-f203-alg21-kem-decaps-device-k2` | Alg.21 合法路径 | CPU+SIM |
| **D21ct** | P2 | W3 | `pass-fix-f203-alg21-kem-decaps-device-ct-k2` | **拒绝路径 / CT** | CPU+SIM；reject≠accept |

### 2.2 incubating — `examples/incubating/ml-kem/ml-kem-512/`

| ID | P | W | 目录 | 作用 | 最小验收 |
|----|---|---|------|------|----------|
| **E13** | P3 | W4a | `exp-fips203-mlkem-pke-keygen-k2` | PKE KeyGen | CPU+SIM；须 customspec |
| **E14** | P3 | W4a | `exp-fips203-mlkem-pke-encrypt-k2` | PKE Encrypt | CPU+SIM |
| **E15** | P3 | W4a | `exp-fips203-mlkem-pke-decrypt-k2` | PKE Decrypt | CPU+SIM |
| **E19** | P3 | W4b | `exp-fips203-mlkem-kem-keygen-k2` | KEM KeyGen | CPU+SIM |
| **E20** | P3 | W4b | `exp-fips203-mlkem-kem-encaps-k2` | KEM Encaps | CPU+SIM |
| **E21** | P3 | W4b | `exp-fips203-mlkem-kem-decaps-k2` | KEM Decaps | CPU+SIM |
| **E21ct** | P3 | W4b | `exp-fips203-mlkem-kem-decaps-ct-k2` | Decaps CT | CPU+SIM |

---

## 3. 波次依赖

```text
W0 (B1–B3b) ──► W1 (B4–B6) ──► W2 (D13–D15) ──► W3 (D19–D21ct)
                                                      │
                                                      ▼
                         W4a (E13–E15) ──► W4b (E19–E21ct) ──► glue
```

---

## 4. 明确不建

- 零垫 / pad-to-3/4/6/8 / limbsplit / NTT S1–S3 `Gather`  
- `examples/stable/ml-kem/ml-kem-512/**`（须 `#交付#`）  
- 从 `**/frozen/**` 抄实现  
- 用 768/1024 fixture 改长度冒充 512  

---

## 5. P1 退出清单

- [x] 缺项对照（补缺图）成文  
- [x] 必建表含 **PKE exp + KEM device + CT**  
- [x] 目录壳与 INDEX 对齐本表  
- [x] 与参数卡交叉引用  
- [ ] P2（W0–W3）与 P3（W4 + glue）  
- [ ] stable-512（须 `#交付#`，非本阶段）
