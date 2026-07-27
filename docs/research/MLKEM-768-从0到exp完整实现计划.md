# ML-KEM-768：从 0 到 incubating exp 完整实现计划

**状态**：**有条件完成（至 incubating exp）**（2026-07-26/27）· P0–P3 主线已走通；**未**建 stable-768 / 未跑 NPU / liboqs-768 交叉仍可选  
**权威参数 / 用例表**：[`docs/specs/fips203-mlkem768-parameter-card.md`](../specs/fips203-mlkem768-parameter-card.md) · [`…-p1-gap-and-cases.md`](../specs/fips203-mlkem768-p1-gap-and-cases.md)  
**范围**：真 **ML-KEM-768**（\(k=3\)）→ incubating **PKE×3 + KEM×3 + decaps-ct**（已绿）  
**不做（本阶段）**：零垫；**不**建 `examples/stable/ml-kem/ml-kem-768/`  
**端到端**：[`scripts/exp_kem768_liboqs_roundtrip.sh`](../../scripts/exp_kem768_liboqs_roundtrip.sh)（当前 **AscendC-only**；CPU×1+SIM×1 已绿）  
**参考（只读模式，不抄实现）**：`…/ml-kem/ml-kem-1024/` 与 `docs/notes/`  
**当日收尾纪要**：[qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md](../../qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md)

---

## 0. 目标与完成判据

| 项 | 约定 |
|----|------|
| **终点** | incubating：**PKE** keygen/encrypt/decrypt + **KEM** keygen/encaps/decaps + **decaps-ct** 均可默认路径验收 |
| **验收** | 各用例目录 **CPU + `SIM_DIRECT=1` sim** 双过；I/O 与 golden（liboqs `ML-KEM-768`）一致；用例根无 stray dump |
| **端到端** | 新增 `scripts/exp_kem768_liboqs_roundtrip.sh`（或等价）：KeyGen→Encaps→Decaps 至少 CPU×1 + SIM×1 |
| **非目标** | stable 晋级、NPU 真机压测、性能打满、教材长文成稿（可另开） |

**一句话**：用真 \(k=3\) 参数组，把「参数卡 → 必改积木 → PKE device → KEM exp」走通；1024 只当已获证**模式**，禁止形状硬套 / 零垫。

---

## 1. 硬约束（开写即锁）

### 1.1 FIPS 203 参数卡（草案，P0 确认后锁定）

| 符号 | ML-KEM-768 | 对照 1024 |
|------|------------|-----------|
| \(n,q\) | 256, 3329 | 同 |
| \(k\) | **3** | 4 |
| \(\eta_1,\eta_2\) | **2, 2** | 2, 2 |
| \(d_u,d_v\) | **10, 4** | 11, 5 |
| \(\lvert ek\rvert=\lvert ek_{\mathrm{PKE}}\rvert\) | \(384k+32=\) **1184 B** | 1568 |
| \(\lvert dk_{\mathrm{PKE}}\rvert\) | \(384k=\) **1152 B** | 1536 |
| \(\lvert dk_{\mathrm{KEM}}\rvert\) | \(768k+96=\) **2400 B** | 3168 |
| \(\lvert c\rvert\) | \(32k\cdot d_u/8+32\cdot d_v/8=\) **1088 B** | 1568 |
| \(\lvert m\rvert,\lvert K\rvert,\lvert \rho\rvert,\lvert z\rvert\) | 32 B | 同 |

> 注意：仓内旧文偶发「ML-KEM-768，\(K=4\)」为**笔误**；本计划一律以 **\(k=3\)** 为准。

### 1.2 工程门禁

| 必须 | 禁止 |
|------|------|
| 代码落在 `ascendc-tests/ml-kem/ml-kem-768/` 与 `examples/incubating/ml-kem/ml-kem-768/` | 在 1024 目录内改 \(k\) 冒充 768 |
| 命名后缀 **`-k3`**（探针 `pass-fix-f203-…-k3`，exp `exp-fips203-…-k3`） | `-k4` 目录改参继续跑 |
| golden / KAT 锚定 **liboqs `ML-KEM-768`**（及登记表允许的 LUT/API） | 用 1024 fixture 改长度验收 |
| AscendC API 用前查查阅索引；自研中文注释同轮 | 从 `**/frozen/**` 抄实现 |
| 已锁形状 / tiling / `blockDim` 遇阻 → **停** 并重开参数讨论 | 擅自改参绕编译 / 对拍 |

### 1.3 相对 1024 的结构性差异（为何必须重做）

1. **\(k=3\) 为奇数**：1024 大量布局按「4 poly / 8 路 s‖e（polyvec8）/ 双 AIV 均分」设计；不能「少算 1 路」或零垫成 4。  
2. **矩阵 \(\hat A\)**：\(3\times3\)（9 poly）而非 \(4\times4\)（16 poly）。  
3. **Compress/ByteEncode**：\(d\in\{10,4\}\) 为主路径（1024 主路径是 11/5）；统一整数舍入表已有 10/4 档，但**封装与对拍**须在 768 树重做。  
4. **I/O 字节契约全变**：一切 `ReadFile`/`cmp`/stash 长度重锁。  
5. **内积 / mat_c 几何**：行数、batch、Stage2 MMAD `SetSingleShape` 必须按 \(k=3\) 重推，禁止复用 k4 tiling 常数。

---

## 2. 总体路线（四阶段）

```text
P0 文书锁定 ──► P1 补洞清单+用例表 ──► P2 按波次实现 ──► P3 exp 三算子闭环
     │                │                      │                    │
  参数卡/CT/        「必建 vs 可省」         分析→文书→码→测         roundtrip
  分核选项           写入本文件确认          每波 CPU+SIM
```

| 阶段 | 产出 | 退出门禁 |
|------|------|----------|
| **P0** | 参数卡 + CT + **分核/tiling 选项决议** + 目录壳 + INDEX | ✅ 已完成（见参数卡） |
| **P1** | 相对 1024 的洞表；必建用例表定稿 | ✅ 已完成（见 P1 文；含 PKE exp + CT） |
| **P2** | 积木 → PKE device → KEM device(+ct) 探针全绿 | ✅ 已完成（W0–W3；2026-07-26） |
| **P3** | PKE+KEM exp(+ct) + 768 roundtrip | ✅ **有条件完成**（W4 + glue；AscendC-only RT；2026-07-26/27） |

**P 与 W**：P0–P3 为方法论主阶段；W0–W4 为 P2/P3 内工程子波次（详见 [P1 用例表 §0](../specs/fips203-mlkem768-p1-gap-and-cases.md)）。表内 **P、W 分列**，勿写 `P2-W0`。

Skill 节奏（开写时）：积木/探针可走 **【预研】**；进 `examples/incubating/…` 写码前须有活跃 **`*-customspec.*`**（`$规格$`）；本阶段不触发 `#交付#` stable。

---

## 3. P0 — 参数卡、CT 与架构选项（先决）

### 3.1 交付物（仅文书 + 空目录壳，可先做）

| ID | 交付物 | 路径建议 |
|----|--------|----------|
| P0-A | **参数卡**（上表 + host/device I/O 清单 + derand 域分离串是否复用） | `docs/specs/fips203-mlkem768-parameter-card.md` |
| P0-B | **CT 对照表**（liboqs 符号名、长度、KAT 入口） | 同上附录或 `…-conformance.md` |
| P0-C | **分核 / tiling 选项决议**（见 §3.2，须用户选一） | 参数卡 §Tiling |
| P0-D | 目录壳 + INDEX 占位 | `ascendc-tests/ml-kem/ml-kem-768/` · `examples/incubating/ml-kem/ml-kem-768/`（**无** stable） |
| P0-E | baseline-registry **草稿框**（六份可先骨架，登记「未验证」块标红） | `docs/specs/fips203-mlkem768-*-baseline-registry.md` |

### 3.2 必须先拍板的架构选项（\(k=3\)）

下列选项影响几乎所有后续探针；**P0 结束前选定主路线**（可标「备选」但不并行实现）。

#### 选项 T — poly 批与 AIV 映射（NTT / KeyGen 核心）

| 代号 | 思路 | 利 | 弊 |
|------|------|----|-----|
| **T-A** | **真 3-poly 批**：Stage 按 \(k=3\) 重切；AIV 负载显式不均（如 2+1）或 3 核参与 | 语义干净，符合「真 768」 | 与 k4 均分模板差最大，工程量高 |
| **T-B** | **6-poly（s‖e）为批单位** 重做「polyvec6」流水（类比 1024 的 polyvec8，但是 6 非 8） | 延续「整 poly 握在同一 AIV」不变量 | 仍须重做 tiling；不能复用 8 路常量 |
| **T-C** | 矩阵按行/列 **串行多 launch** 降低单核形状难度 | 降低首版 SIM 风险 | launch 数↑；须在 customspec 写清，避免日后误当性能基线 |

**已锁定（用户同意）**：**T-B**（polyvec6）+ \(\hat A\) 独立 prep；**明确拒绝** pad 到 4/8。

#### 选项 C — Compress / ByteEncode

| 代号 | 思路 |
|------|------|
| **C-1** | 在 768 树新建 `pass-f203-compress-d-vec-k3` / `decompress` / `byteencode-d`，CMake 默认 **d∈{10,4}**（可切 12 做密钥域） |
| **C-2** | 先晋级/复用 incubating `exp-…-compress-unified-int-*` 的**算法**，但 **I/O 与用例必须在 768 树重挂**（禁止 1024 目录改默认 d） |

**推荐**：**C-1**（树干净）；算法可对照 notes「统一整数舍入」的 d=10/4 行，**不抄 1024 源码当规格**。

#### 选项 L — Launch 分叉

沿用仓规：CPU 可多 launch 对照；SIM/生产默认少 launch / 融合路径写进各 customspec。768 首版允许 SIM 先「正确性优先」的 launch 数，但须在 STATUS 标 **非性能基线**。

### 3.3 P0 测试（无 kernel 也可做）

- [ ] 参数卡长度与 liboqs `OQS_KEM_ml_kem_768_*` 宏交叉核对脚本（小 Python，放 `scripts/` 或参数卡附录）
- [ ] `thirdparty`：确认 liboqs 启用 ML-KEM-768；`clone-thirdparty` / kem_ref 可编过
- [ ] 幽灵策略：新建目录后跑 `cleanup-ascendc-test-ghosts.sh` 基线

---

## 4. P1 — 补洞图与「必建用例」清单

### 4.1 相对 1024：可继承 vs 必须重做

| 能力块 | 1024 现状 | 768 策略 |
|--------|-----------|----------|
| SHAKE / Keccak 设备 | shared 已有 | **继承**（仅改调用次数/长度） |
| CBD \(\eta=2\) 单 poly 向量 | `pass-fix-…-alg8-cbd-eta2-k4` | **轻量复测**：新探针或同逻辑挂 k3 packing（§4.2 B2） |
| NTT 单 poly 数学 | notes 定稿 | **继承数学**；**设备布局重做** |
| Compress d=11/5 | 1024 主路径 | **不直接用**；改 d=10/4 |
| polyvec8 / k4 内积 | 基线探针 | **整段作废为实现模板**；只借不变量叙述 |
| FO / G / H / J | KEM notes | **继承算法**；I/O 长度与 stash 重做 |
| CPU/SIM 分叉、camodel 日志 | engineering | **继承工程壳** |

### 4.2 必建用例表（P1 定稿对象；自行补充的 P1/P2 级）

> 原则：**不**克隆 1024 全部 ~27 探针；只建「参数变化会导致错误的最小闭环」。中间分段探针（如仅 lines3–7）默认 **不建**，除非 P2 某波卡死再加。

#### W0（P2）— 编码与压缩（P1 积木）

| ID | P | W | 目录（建议名） | 作用 | 验收 |
|----|---|---|----------------|------|------|
| **B1** | P2 | W0 | `pass-f203-compress-decompress-du10-dv4-k3` | Compress/Decompress \(d_u=10,d_v=4\)（可单目录多 target） | CPU+SIM；与 Python/liboqs 定点一致 |
| **B2** | P2 | W0 | `pass-f203-byteencode-decode-d-k3` | ByteEncode/Decode：至少 **d=10,4,12** | CPU+SIM；长度公式锁死 |
| **B3** | P2 | W0 | `pass-fix-f203-alg8-cbd-eta2-k3` | CBD \(\eta=2\) × **3 poly 打包**（验证 k 维布局，非算法新发明） | CPU+SIM |

#### W1（P2）— 采样与 NTT（P1→P2 核心风险）

| ID | P | W | 目录（建议名） | 作用 | 验收 |
|----|---|---|----------------|------|------|
| **B4** | P2 | W1 | `pass-fix-f203-alg7-sample-ntt-k3` | SampleNTT；矩阵索引 \(i,j\in\{0,1,2\}\)，覆盖 \(3\times3\) | CPU+SIM；多 seed |
| **B5** | P2 | W1 | `pass-fix-f203-stage123-ntt-intt-polyvec-k3` | Stage1–3 NTT/INTT，**真 k=3 / polyvec6 语义**（禁 Gather 于 S1–S3，沿用 Tag5T 不变量） | CPU+SIM；与 liboqs/ref 系数域对拍 |
| **B6** | P2 | W1 | `pass-fix-f203-alg11-12-multiply-inner-k3` | MultiplyNTTs + InnerProduct（\(k=3\) 维） | CPU+SIM；可为合并探针以省目录 |

#### W2（P2）— PKE device

| ID | P | W | 目录（建议名） | 作用 | 验收 |
|----|---|---|----------------|------|------|
| **D13** | P2 | W2 | `pass-fix-f203-alg13-device-keygen-k3` | Alg.13 全链 → `ek_pke`/`dk_pke` | CPU+SIM；liboqs 交叉 |
| **D14** | P2 | W2 | `pass-fix-f203-alg14-pke-encrypt-device-k3` | Alg.14 → `c`（1088 B） | CPU+SIM |
| **D15** | P2 | W2 | `pass-fix-f203-alg15-pke-decrypt-device-k3` | Alg.15 → `m` | CPU+SIM；与 D14 roundtrip |

#### W3（P2）— KEM 行为基线（建议要）

| ID | P | W | 目录（建议名） | 作用 | 验收 |
|----|---|---|----------------|------|------|
| **D19** | P2 | W3 | `pass-fix-f203-alg19-kem-keygen-device-k3` | Alg.19；产出 ek/dk_kem | CPU+SIM |
| **D20** | P2 | W3 | `pass-fix-f203-alg20-kem-encaps-device-k3` | Alg.20；`c`/`K` | CPU+SIM |
| **D21** | P2 | W3 | `pass-fix-f203-alg21-kem-decaps-device-k3` | Alg.21 合法路径 `K` | CPU+SIM |
| **D21ct** | P2 | W3 | `pass-fix-f203-alg21-kem-decaps-device-ct-k3` | 拒绝路径 / CT 工程对照（**强烈建议**，与 1024 经验一致） | CPU+SIM；reject≠accept |

> 若进度极紧，可论证 **D19–D21 与 exp 合并**（device 探针只留 D13–D15）。**默认建议保留 D19–D21**：exp 写码前先有行为基线，避免 FO 与 PKE 耦合一次炸。

#### W4（P3）— incubating exp（终点）

| ID | P | W | 目录（建议名） | customspec | 验收 |
|----|---|---|----------------|------------|------|
| **E19** | P3 | W4b | `exp-fips203-mlkem-kem-keygen-k3` | 必有 | CPU+SIM；registry 已填 |
| **E20** | P3 | W4b | `exp-fips203-mlkem-kem-encaps-k3` | 必有 | CPU+SIM |
| **E21** | P3 | W4b | `exp-fips203-mlkem-kem-decaps-k3` | 必有 | CPU+SIM；建议含 reject 开关 |
| **E21ct** | P3 | W4b | `exp-…-kem-decaps-ct-k3`（可选） | 若 D21ct 路径要晋级再开 | 非终点必达 |

**PKE 的 exp**（`exp-…-pke-*-k3`）：**用户要求必做**（E13–E15）；另加 **E21ct**。详见 P1 表。

### 4.3 明确不建（除非卡死再开）

- 所有 `frozen-*` 继任抄码  
- 零垫 / limbsplit / se_pair / NTT 内 Gather  
- 1024 式细切：`lines3-7` / `lines8-15` / `encrypt-pack` 独立探针（逻辑内嵌 D13/D14）  
- `examples/stable/ml-kem/ml-kem-768/**`  
- 全量 msprof 性能门禁（仅记录墙钟作参考）

---

## 5. P2 — 按波次实现节奏

每一波固定流水（不可跳门禁）：

```text
分析（形状/同步/I/O）→ 文书（INTEGRATION_PLAN 或 customspec）→ 实现 → 
bash run.sh -r cpu -v Ascend910B4 →
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4 →
打勾 STATUS + 当日 qa 追加
```

### 5.1 波次依赖

```text
W0 (B1–B3) ─┬─► W1 (B4–B6) ─► W2 (D13–D15) ─► W3 (D19–D21[+ct]) ─► W4 (E19–E21)
            │         ▲
            └─────────┘  B1/B2 在 D14/D15 pack 前必须绿
```

### 5.2 各波关键风险与停问点

| P | W | 最高风险 | 停问条件 |
|---|---|----------|----------|
| P2 | W0 | d=10/4 定点与 liboqs 一致性 | 对拍差 1 ULP 级争议 |
| P2 | W1 | **奇数 k 分核 / mat_c 几何** | 任何「先 pad 成 4」的诱惑 → 必须停 |
| P2 | W2 | prep+compute+pack 融合与 SIM session | launch 数要改参数卡 |
| P2 | W3 | FO、reject、密文长 1088 | `M_FILE`/`C_SRC` 语义歧义 |
| P3 | W4 | customspec 与 device 行为漂移 | exp 与 D19–D21 I/O 不一致 |

### 5.3 测试矩阵（最小）

| 对象 | CPU | SIM_DIRECT | liboqs 交叉 | 备注 |
|------|-----|------------|-------------|------|
| B1–B6 | ✓ | ✓ | 能则做 | B5/B6 必须有 ref |
| D13–D15 | ✓ | ✓ | ✓ | D14↔D15 自 roundtrip |
| D19–D21 | ✓ | ✓ | ✓ | D21ct reject |
| E19–E21 | ✓ | ✓ | ✓ | + `exp_kem768_liboqs_roundtrip` |
| 幽灵扫描 | 每波结束 | — | — | `cleanup-ascendc-test-ghosts.sh` |

KAT 批次数（建议，非 stable 门禁）：device/exp 各 **CPU×3 + SIM×1** 起步；卡 flaky 再加。

---

## 6. P3 — exp 闭环与「完成」定义

### 6.1 exp 三算子最低 I/O

| 算子 | 输入 | 输出 |
|------|------|------|
| KeyGen | `seed_d`（+LUT 若需） | `ek_kem`(1184)、`dk_kem`(2400) |
| Encaps | `ek_kem`、`m` | `c`(1088)、`K`(32) |
| Decaps | `dk_kem`、`c` | `K`(32) |

### 6.2 完成声明模板（供收工时用）

> **有条件完成（至 incubating exp / ML-KEM-768）**：E19–E21 CPU+SIM 绿；768 liboqs roundtrip 绿；参数卡与实现一致；未做 stable / NPU。

### 6.3 显式不做的后续（另开任务）

- `#交付#` → stable-768  
- 性能定标（类 NTT ~15s 那套 **不**自动套用）  
- ML-KEM-512（\(k=2,\eta_1=3\)）  

---

## 7. 文档与索引义务

| 时机 | 动作 |
|------|------|
| P0 建目录 | 更新 `ascendc-tests/ml-kem/INDEX.md`、`examples/incubating/ml-kem/INDEX.md`；**不**建 stable-768 |
| 每波 | 用例 `STATUS.md`；当日 `qa/YYYY-MM-DD-*.md` 追加 |
| 登记表缺块 | **停**，补 registry 或退回预研 |
| 定稿原理 | 768 特有结论（奇数 k 分核等）→ 日后 `docs/notes/`；过程留 `docs/research/` |

---

## 8. 建议排期节奏（按技术依赖，非日历）

| 序 | P | W | 内容 | 退出 |
|----|---|---|------|------|
| 1 | — | — | 你审阅本计划；确认 §3.2 **T/\*\*C** 选项与 §4.2 必建表 | 书面 OK |
| 2 | P0 | — | P0 文书 + 目录壳 + 长度核对脚本 | 参数锁定 |
| 3 | P2 | W0→W1 | 积木双绿 | B1–B6 双绿 |
| 4 | P2 | W2 | PKE device 三绿 + PKE roundtrip | D13–D15 |
| 5 | P2 | W3 | KEM device（+ct）绿 | D19–D21ct |
| 6 | P3 | W4+glue | exp 三算子 + 768 roundtrip；刷新 HANDOFF | 有条件完成 |

---

## 9. 决议记录（2026-07-26）

| # | 决议 |
|---|------|
| 1 | 同意 **T-B + Â prep** |
| 2 | **保留** D19–D21 |
| 3 | **要求** reject/CT（D21ct + E21ct） |
| 4 | **PKE exp 也要做** |
| 5 | 命名 **`-k3` OK** |
| 6 | 先完成 **P0+P1**（本轮已落地文书与目录壳） |

**下一刀**：用户授权后开 **P2、W0**（须按 Skill：【预研】与/或 `$规格$`）。

---

## 10. 参考路径（只读）

- 交接：[`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md)  
- 1024 树：`ascendc-tests/ml-kem/ml-kem-1024/` · `examples/*/ml-kem/ml-kem-1024/`  
- NTT：`docs/notes/MLKEM-NTT-实现总结.md` · `…向量与标量实现指南.md`  
- Compress：`docs/notes/F203-Compress-Decompress-向量实现指南.md` · `…统一整数舍入…`  
- KEM：`docs/notes/F203-KEM-Alg19/20/21-*.md`  
- 1024 registry（模板，不可当 768 真源）：`docs/specs/fips203-mlkem1024-*-baseline-registry.md`
