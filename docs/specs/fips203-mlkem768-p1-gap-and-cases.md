# ML-KEM-768 P1：缺项对照（补缺图）与必建用例表（已定稿）

**状态**：**P1 定稿**（2026-07-26）；**实现有条件完成至 incubating**（2026-07-26/27，见当日纪要）  
**依据**：[参数卡](fips203-mlkem768-parameter-card.md) · [完整计划](../research/MLKEM-768-从0到exp完整实现计划.md)  
**下一阶段（可选）**：T768-post / `#交付#` stable-768；本文件不授权跳过 customspec

### 用语（勿另造词）

以教材为准：**缺项** = 闭包中尚未获证的先决；**补缺** = 把缺项变成已获证的过程（旧称「补洞」，已废）；本文「补缺图」= 缺项与补齐策略对照表。  
**禁止**另立「洞 / 新洞 / 洞级 / 补洞」等说法。

---

## 0. P 与 W 编号约定

| 符号 | 含义 |
|------|------|
| **P0–P3** | 方法论主阶段（文书 → 缺项对照/补缺图 → 探针实现 → incubating 闭环） |
| **W0–W4** | **P2/P3 内的工程执行子波次**（积木 → device → exp）；**不是**另一套 P 定义 |
| **glue** | **P3** 收尾：registry 补登 + AscendC-only roundtrip（无独立 W 序号） |

| P | W | 内容 |
|---|---|------|
| P0 | — | 参数卡、CT、目录壳、registry 骨架 |
| P1 | — | 缺项对照（补缺图）、必建用例表定稿 |
| P2 | W0 | 编码/压缩积木 B1–B3 |
| P2 | W1 | 采样/NTT/内积 B4–B6 |
| P2 | W2 | PKE device D13–D15 |
| P2 | W3 | KEM device D19–D21ct |
| P3 | W4 | incubating E13–E21ct |
| P3 | glue | registry + `exp_kem768_liboqs_roundtrip.sh` |

下文用例表 **P、W 分列**；禁止写成 `P2-W0` / `P2/W0` 合并格。

---

## 1. 相对 ML-KEM-1024 的缺项对照（补缺图）

| 能力块 | 1024（参考模式） | 768 策略（补缺） | 备注（高/中/低，非正式） |
|--------|------------------|----------|------|
| SHAKE / Keccak 设备 | `library/shared` | **继承** API；改长度/次数 | 低 |
| CBD \(\eta=2\) 单 poly | `…-alg8-cbd-eta2-k4` | **轻量重挂**：验证 **3/6 poly 打包** | 中 |
| NTT 数学契约 | notes 定稿 | **继承数学**；设备布局重做 | — |
| polyvec8 / k4 内积几何 | 基线探针 | **整段不可当实现模板**；只借不变量叙述 | **高** |
| Compress \(d_u/d_v\) | 主路径 11/5 | **重做** 10/4（C-1） | **高** |
| ByteEncode/Decode | d=4/5/10/11 探针有 | 768 主路径 **10/4 + 12**；树内自建 | 中高 |
| SampleNTT 矩阵 | \(4\times4\) | **\(3\times3\)** | **高** |
| Alg.13–15 device | k4 全链 | **重做 I/O 与 tiling** | **高** |
| Alg.19–21 + CT | k4 + ct | **保留 device + ct**；长度 1184/2400/1088 | **高** |
| FO / G / H / J | notes | **继承算法**；I/O/stash 重做 | 中 |
| PKE/KEM incubating exp | 六算子 + ct | **PKE×3 + KEM×3 + decaps-ct** | 终点 |
| stable | 已有 | **本阶段不做** | — |
| 零垫 / limbsplit / NTT-Gather | frozen/禁令 | **继续禁止** | — |

---

## 2. 必建用例总表（已锁）

目录与用例均已落盘；实现状态见各树 `INDEX.md` / `STATUS.md`（探针 W0–W3、incubating W4 均已绿）。

### 2.1 探针 — `ascendc-tests/ml-kem/ml-kem-768/`

| ID | P | W | 目录 | 作用 | 最小验收 |
|----|---|---|------|------|----------|
| **B1** | P2 | W0 | `pass-fix-f203-compress-decompress-du10-dv4-k3` | Compress/Decompress \(d_u=10,d_v=4\) | CPU+SIM；定点 vs oracle |
| **B2** | P2 | W0 | `pass-fix-f203-byteencode-decode-d-k3` | ByteEncode/Decode：至少 d=10,4,12 | CPU+SIM |
| **B3** | P2 | W0 | `pass-fix-f203-alg8-cbd-eta2-k3` | CBD \(\eta=2\) × k 维打包 | CPU+SIM |
| **B4** | P2 | W1 | `pass-fix-f203-alg7-sample-ntt-k3` | SampleNTT；\(i,j\in\{0,1,2\}\) | CPU+SIM；多 seed |
| **B5** | P2 | W1 | `pass-fix-f203-stage123-ntt-intt-polyvec6-k3` | Stage1–3；**真 polyvec6** | CPU+SIM；禁 S1–S3 Gather |
| **B6** | P2 | W1 | `pass-fix-f203-alg11-12-multiply-inner-k3` | MultiplyNTTs + InnerProduct（\(k=3\)） | CPU+SIM |
| **D13** | P2 | W2 | `pass-fix-f203-alg13-device-keygen-k3` | Alg.13 → ek/dk_pke | CPU+SIM；liboqs 交叉 |
| **D14** | P2 | W2 | `pass-fix-f203-alg14-pke-encrypt-device-k3` | Alg.14 → c(1088) | CPU+SIM |
| **D15** | P2 | W2 | `pass-fix-f203-alg15-pke-decrypt-device-k3` | Alg.15 → m | CPU+SIM；与 D14 RT |
| **D19** | P2 | W3 | `pass-fix-f203-alg19-kem-keygen-device-k3` | Alg.19 | CPU+SIM |
| **D20** | P2 | W3 | `pass-fix-f203-alg20-kem-encaps-device-k3` | Alg.20 | CPU+SIM |
| **D21** | P2 | W3 | `pass-fix-f203-alg21-kem-decaps-device-k3` | Alg.21 合法路径 | CPU+SIM |
| **D21ct** | P2 | W3 | `pass-fix-f203-alg21-kem-decaps-device-ct-k3` | **拒绝路径 / CT** | CPU+SIM；reject≠accept |

### 2.2 incubating exp — `examples/incubating/ml-kem/ml-kem-768/`

| ID | P | W | 目录 | 作用 | 最小验收 |
|----|---|---|------|------|----------|
| **E13** | P3 | W4a | `exp-fips203-mlkem-pke-keygen-k3` | PKE KeyGen exp | CPU+SIM；须 customspec |
| **E14** | P3 | W4a | `exp-fips203-mlkem-pke-encrypt-k3` | PKE Encrypt exp | CPU+SIM |
| **E15** | P3 | W4a | `exp-fips203-mlkem-pke-decrypt-k3` | PKE Decrypt exp | CPU+SIM |
| **E19** | P3 | W4b | `exp-fips203-mlkem-kem-keygen-k3` | KEM KeyGen exp | CPU+SIM |
| **E20** | P3 | W4b | `exp-fips203-mlkem-kem-encaps-k3` | KEM Encaps exp | CPU+SIM |
| **E21** | P3 | W4b | `exp-fips203-mlkem-kem-decaps-k3` | KEM Decaps exp | CPU+SIM |
| **E21ct** | P3 | W4b | `exp-fips203-mlkem-kem-decaps-ct-k3` | Decaps CT / reject | CPU+SIM |

**端到端脚本（P3）**：`scripts/exp_kem768_liboqs_roundtrip.sh`（KeyGen→Encaps→Decaps；建议另含 reject 抽检）。

---

## 3. 波次依赖与测试矩阵

```text
W0 (B1–B3) ──► W1 (B4–B6) ──► W2 (D13–D15) ──► W3 (D19–D21+ct)
                                                      │
                                                      ▼
                         W4a (E13–E15) ──► W4b (E19–E21+ct) ──► roundtrip
```

| 对象 | CPU | `SIM_DIRECT=1` sim | liboqs 交叉 | 备注 |
|------|-----|---------------------|-------------|------|
| B1–B6 | ✓ | ✓ | 能则做 | B5/B6 必须有 ref |
| D13–D15 | ✓ | ✓ | ✓ | D14↔D15 自 RT |
| D19–D21+ct | ✓ | ✓ | ✓ | ct：reject≠accept |
| E13–E15 | ✓ | ✓ | ✓ | 须活跃 customspec |
| E19–E21+ct | ✓ | ✓ | ✓ | + 768 roundtrip |
| 幽灵 | 每波末 | — | — | `cleanup-ascendc-test-ghosts.sh` |

KAT 起步建议：device/exp **CPU×3 + SIM×1**（非 stable 门禁）。

---

## 4. 明确不建（P1 锁定）

- 1024 细切独立探针（`lines3-7` / `lines8-15` / `encrypt-pack` 等）— 逻辑内嵌 D13/D14  
- 零垫 / pad-to-4/8 / limbsplit / NTT S1–S3 `Gather`  
- `examples/stable/ml-kem/ml-kem-768/**`  
- 从 `**/frozen/**` 抄实现  

若 P2 卡死需要加细切探针：先改本表与参数卡，再开目录。

---

## 5. P1 退出清单

- [x] 缺项对照（补缺图）成文  
- [x] 必建表含 **PKE exp + KEM device + CT**（用户决议）  
- [x] 目录壳与 INDEX 对齐本表  
- [x] 与参数卡交叉引用  
- [x] P2（W0–W3）与 P3（W4 + glue）已按授权完成（2026-07-26/27）  
- [ ] stable-768（须 `#交付#`）  
- [ ] T768-post（liboqs-768 helper / device KAT 等，可选）
