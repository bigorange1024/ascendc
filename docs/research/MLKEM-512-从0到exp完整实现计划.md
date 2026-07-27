# ML-KEM-512：从 0 到 incubating exp 完整实现计划

**状态**：**有条件完成**（2026-07-27；P0–P3 W0–W4+glue 全绿；禁 stable-512）  

**参数组**：真 **ML-KEM-512**（k=2,\eta_1=3,\eta_2=2,d_u=10,d_v=4）  
**实验意图**：**方法论压力床 3**——参照 768 闭环，在用户**极少介入**下自主推进至 incubating + 测试 + 文档刷新  
**权威参数卡**：[docs/specs/fips203-mlkem512-parameter-card.md](../specs/fips203-mlkem512-parameter-card.md)（**已锁**）  
**对照（已完成）**：[MLKEM-768-从0到exp完整实现计划.md](MLKEM-768-从0到exp完整实现计划.md) · [P1 用例表 §0 P/W 约定](../specs/fips203-mlkem768-p1-gap-and-cases.md)  
**参考（只读模式，不抄实现）**：`…/ml-kem/ml-kem-768/`、`…/ml-kem/ml-kem-1024/`、`docs/notes/`  
**不做（本阶段默认）**：零垫；**不**建 `examples/stable/ml-kem/ml-kem-512/`（须 `#交付#`）；**不**从 `**/frozen/`** 抄码  

### 用语（勿另造词）

以教材已定义为准（见 `docs/research/从已验证能力到合法派生-….tex`）：

| 已定义 | 含义（工程口述） |
|--------|------------------|
| **缺项** | 依赖闭包里尚未获证的先决（\(\mathrm{Missing}_\Gamma\)） |
| **补缺** | 把缺项逐步变成已获证、使目标闭包已满的过程 |
| **补缺图** | P1 工程表：列出缺项与补齐策略（**不是**新对象） |
| **闭包 / 已获证 / 最小补缺** | 同教材 |

只使用上表已定义术语；**勿另造**无定义同义词。registry「缺项」仍指登记表未覆盖的计算块（Rule 原义）。

---



## 0. 目标与完成判据


| 项          | 约定                                                                                                      |
| ---------- | ------------------------------------------------------------------------------------------------------- |
| **终点**     | incubating：**PKE**×3 + **KEM**×3 + **decaps-ct** 均可默认路径验收                                               |
| **验收**     | 各用例 **CPU +** `SIM_DIRECT=1` **sim** 双过；I/O 与 golden（liboqs `ML-KEM-512`）一致；用例根无 stray dump             |
| **端到端**    | `scripts/exp_kem512_*_roundtrip.sh`（至少 AscendC-only KeyGen→Encaps→Decaps；含 reject 抽检）                   |
| **交叉 KAT** | **本阶段目标内**：device/exp 对 liboqs-512 derand/KAT 至少 CPU×N + SIM×1（N 见 §6）；**KEM helper 已支持 512**（见 P0-G） |
| **性能**     | 首绿以正确性为准；同波次内消除明显标量 GM / 多余同步大头；SIM tick **相对 768 同算子应更低或同量级**（k=2）；挂死超时遵守各 `run.sh` 预算                 |
| **文档**     | 参数卡 / P1 表 / registry / 两树 INDEX / STATUS / 当日 `qa/` / `AGENT_HANDOFF`；定稿教训择要入 `docs/notes/` 或教材续章（可后置） |
| **非目标**    | stable 晋级、NPU、教材第9章长文（除非用户另开）                                                                           |


**一句话**：用真 k=2 + **单 AI Core** 几何，把 768 已验证的「参数卡 → 补缺 → 波次绿线 → incubating → glue」再跑通一遍，检验方法论能否指导下一参数组。

---



## 0.1 自主实验协议（用户一次拍板后）


| 角色        | 做什么                                                              |
| --------- | ---------------------------------------------------------------- |
| **用户**    | 锁本计划 §3.2 选项 + §9 决议表；授权「按本计划自主推进至有条件完成」                         |
| **Agent** | 自写 P1 定稿表、目录壳、customspec、实现、双模式验收、liboqs 交叉（若门禁允许）、roundtrip、刷文档 |
| **仅停问**   | 锁参冲突；单核选项证明不可行须改路线；registry 缺计算块且无合法来源；与 Rule/Skill 冲突           |


**禁止静默**：改已锁 k/\eta/d_u/d_v/I/O 长度；零垫；抄 frozen；无 customspec 改 `examples/`；建 stable-512。

---



## 0.2 P 与 W 编号（与 768 同构）


| 符号        | 含义                                                     |
| --------- | ------------------------------------------------------ |
| **P0–P3** | 方法论主阶段                                                 |
| **W0–W4** | **P2/P3 内**工程子波次；**不是**另一套 P                           |
| **glue**  | P3 收尾：registry + roundtrip（+ liboqs-512 helper/KAT 胶水） |



| P   | W    | 内容                                                  |
| --- | ---- | --------------------------------------------------- |
| P0  | —    | 参数卡、CT、**单核选项**、目录壳、registry 骨架、尺寸自检脚本              |
| P1  | —    | 相对 768/1024 **缺项对照（补缺图）**；必建用例表定稿                             |
| P2  | W0   | 编码/压缩 + **CBD \eta=3**（相对 768 的缺项）积木                           |
| P2  | W1   | SampleNTT 2\times2 + NTT/INTT（单核几何）+ Multiply/Inner |
| P2  | W2   | PKE device D13–D15                                  |
| P2  | W3   | KEM device D19–D21ct                                |
| P3  | W4   | incubating E13–E21ct                                |
| P3  | glue | registry + AscendC RT + **liboqs-512 KAT**          |


表内 **P、W 分列**；禁止 `P2-W0` 合并格。

---



## 1. 硬约束（开写即锁）



### 1.1 FIPS 203 / liboqs 标量参数


| 符号                                   | ML-KEM-512 | 对照 768 | 对照 1024 |
| ------------------------------------ | ---------- | ------ | ------- |
| n,q                                  | 256, 3329  | 同      | 同       |
| k                                    | **2**      | 3      | 4       |
| \eta_1,\eta_2                        | **3, 2**   | 2, 2   | 2, 2    |
| d_u,d_v                              | **10, 4**  | 10, 4  | 11, 5   |
| \lvert ek\rvert                      | **800 B**  | 1184   | 1568    |
| \lvert dk_{\mathrm{PKE}}\rvert       | **768 B**  | 1152   | 1536    |
| \lvert dk_{\mathrm{KEM}}\rvert       | **1632 B** | 2400   | 3168    |
| \lvert c\rvert                       | **768 B**  | 1088   | 1568    |
| \lvert m\rvert,\lvert K\rvert,\ldots | 32 B       | 同      | 同       |


已与 `thirdparty/liboqs/.../kem_ml_kem.h` 宏对拍：`public_key=800`、`secret_key=1632`、`ciphertext=768`。

展开：

```text
ek_kem == ek_pke                         # 800
dk_pke = ByteEncode_12(ŝ)                # 768 = 12*k*n/8
dk_kem = dk_pke ‖ ek_kem ‖ H(ek) ‖ z     # 768+800+32+32 = 1632
c = c1 ‖ c2                              # du*k*n/8 + dv*n/8 = 640+128 = 768
```



### 1.2 工程门禁


| 必须                                                                                 | 禁止                                             |
| ---------------------------------------------------------------------------------- | ---------------------------------------------- |
| 代码落在 `ascendc-tests/ml-kem/ml-kem-512/` 与 `examples/incubating/ml-kem/ml-kem-512/` | 在 768/1024 目录改 k 冒充 512                        |
| 命名后缀 `-k2`                                                                         | `-k3`/`-k4` 改参继续跑                              |
| golden / KAT 锚定 **liboqs** `ML-KEM-512`                                            | 用 768/1024 fixture 改长度验收                       |
| AscendC API 用前查查阅索引；自研中文注释同轮                                                       | 从 `**/frozen/**` 抄实现（含历史 poly2 / merged_kyber） |
| 已锁形状 / tiling / `blockDim` 遇阻 → **停**                                              | 擅自改参绕编译 / 对拍                                   |
| **单 AI Core** 语义以 §3.2 锁定为准                                                        | 静默回到多 cube 分核「先跑通再说」                           |




### 1.3 相对 768 / 1024 的结构性差异

1. **k=2**（偶数）：矩阵 \hat A 为 2\times2（4 poly）；噪声侧 s‖e 为 **polyvec4**（非 6/8）。
2. **\eta_1=3**：**CBD‑3 新积木**——768/1024 主路径只有 \eta=2；不可「改长度」混过。
3. **d_u,d_v=10,4**：与 768 同档；Compress/ByteEncode **可复用算法模式**，但须在 **512 树**重挂 I/O 与对拍。
4. **单 AI Core（本实验核心）**：分核/同步/launch 与 768 的「多 AIV 切 poly 批」**不是改 k**；须按 §3.2 重推。
5. **I/O 全变**：一切 `ReadFile`/`cmp`/stash 长度重锁（800/1632/768）。
6. **禁零垫**：不得 pad 到 3/4/6/8 冒充已有几何。

---



## 2. 总体路线（四阶段）

```text
P0 文书锁定 ──► P1 缺项对照（补缺图）+用例表 ──► P2 按波次实现 ──► P3 exp + glue
     │                │                   │                    │
  参数卡/单核/      必建 vs 可省         每波 CPU+SIM      RT + liboqs-512 KAT
  目录壳/helper
```


| 阶段     | 产出                                                                                    | 退出门禁                      |
| ------ | ------------------------------------------------------------------------------------- | ------------------------- |
| **P0** | 参数卡 + 单核决议 + 目录壳 + registry 骨架 + `check_mlkem512_sizes.sh` +（若需）liboqs-512 helper 可切换 | 用户锁 §9                    |
| **P1** | 缺项对照（补缺图）；必建用例表（另文 `…-p1-gap-and-cases.md`）                                                 | 表定稿                       |
| **P2** | W0–W3 探针全绿                                                                            | 每波 CPU+`SIM_DIRECT=1` sim |
| **P3** | W4 incubating + glue（RT + liboqs KAT）                                                 | **有条件完成**                 |


Skill：探针可走【预研】；`examples/incubating/` 写码前须活跃 `*-customspec.*`；本阶段不触发 `#交付#`。

---



## 3. P0 — 参数卡、单核与目录壳



### 3.1 交付物


| ID   | 交付物                        | 路径建议                                                                                        |
| ---- | -------------------------- | ------------------------------------------------------------------------------------------- |
| P0-A | 参数卡（本文件 §1 + I/O + tiling） | `docs/specs/fips203-mlkem512-parameter-card.md`                                             |
| P0-B | CT / 长度对照（并入参数卡）           | 同上                                                                                          |
| P0-C | **单 AI Core 选项决议**（§3.2）   | 参数卡 §Tiling                                                                                 |
| P0-D | 目录壳 + INDEX                | `ascendc-tests/ml-kem/ml-kem-512/` · `examples/incubating/ml-kem/ml-kem-512/`（**无** stable） |
| P0-E | registry×6 骨架              | `docs/specs/fips203-mlkem512-*-baseline-registry.md`                                        |
| P0-F | 尺寸自检                       | `scripts/check_mlkem512_sizes.sh`                                                           |
| P0-G | liboqs 参数组可切换（KAT/RT 前置） | **KEM 已做**（2026-07-27）：`MLKEM_PARAM=512\|768\|1024`；冒烟绿。**PKE** 仅 1024：`liboqs_pke_ref_mlkem1024`（已改名标明范围；本阶段不对 512/768 测 PKE） |




### 3.2 必须先拍板：单 AI Core（选项 S）

**问题**：用户要求「单 AI Core」。910B 一个 cube ≈ **1 AIC + 2 AIV**。须明确粒度。


| 代号            | 含义                                                                                         | 利                    | 弊                         |
| ------------- | ------------------------------------------------------------------------------------------ | -------------------- | ------------------------- |
| **S-1（推荐草案）** | **单 cube**：MIX/生产路径 `blockDim=1`（一颗 AI Core）；cube 内仍可用 **2 AIV** 做向量流水；**禁止**跨 cube 切 poly | 贴近「单核」产品语义；保留 AIV 并行 | 仍要设计 2 AIV 负载（如 1+1 poly） |
| **S-2**       | **单 AIV 串行**：强制只用一个 AIV 子块跑完全部 poly；另一 AIV idle 或只做辅助                                      | 实现最简、易推理             | 性能上限低；与「性能还得不错」张力大        |
| **S-3**       | **多 launch 串行降形状**：形状难时拆 launch，但仍单 cube                                                   | 降低首版 SIM 风险          | launch 数↑；STATUS 标非性能基线   |


**噪声批单位（与 S 正交，建议锁定）**：


| 代号           | 思路                                                                 |
| ------------ | ------------------------------------------------------------------ |
| **T-B2（推荐）** | **polyvec4**（s‖e，4 poly）为噪声批；\hat A（4 poly）**独立 prep**；禁 pad 到 6/8 |
| **T-A2**     | 真 2-poly 批更细切                                                      |


**NTT 不变量（继承 Tag5T，与 k 无关）**：每个参与计算的 AIV **握完整 poly 的 hi+lo**（禁 limbsplit）；S1–S3 **禁** `Gather`；平面 mat_c。

> **已锁（2026-07-27）**：**S-1 + T-B2 + C-1（512 树自建 compress/byteencode）+ 保留 D19–D21 + CT + PKE exp 必做 +** `-k2`。



### 3.3 Compress / Launch（已锁默认）


| 项                   | 默认                                    |
| ------------------- | ------------------------------------- |
| Compress/ByteEncode | **C-1**：512 树自建；主路径 d\in10,4，密钥域 d=12 |
| Launch              | CPU 可多分段；SIM/生产少 launch；首版正确性优先       |
| 零垫                  | **禁止**                                |
| stable-512          | **本阶段不建**                             |


---



## 4. P1 — 缺项对照与必建用例（草案；定稿另文）



### 4.1 缺项对照（相对 768；即补缺图）


| 能力块                | 768                | 512 策略（补缺）                             | 备注（高/中/低，非正式） |
| ------------------ | ------------------ | ---------------------------------- | --------- |
| SHAKE / Keccak     | shared             | **继承**；改次数/长度                      | 低         |
| CBD \eta=2         | 有                  | Encrypt 噪声等仍要；**轻量重挂 k=2 packing** | 中         |
| CBD \eta=3         | **无**（缺项）          | **新建** Alg.8 \eta=3 探针（KeyGen 主路径） | **高**     |
| Compress 10/4      | 有模式                | 512 树重挂 + 对拍                       | 中         |
| SampleNTT          | 3\times3           | **2\times2**                       | 中高        |
| NTT / 内积几何         | polyvec6 / 多 AIV 切 | **单 cube + polyvec4** 重推           | **高**     |
| PKE/KEM device+exp | 全套                 | **同构重建** I/O 800/1632/768          | **高**     |
| liboqs 交叉          | 768 未全做            | **本阶段要做**（helper）                  | **高（工程）** |
| frozen poly2 等     | —                  | **只读判决，不抄码**                       | —         |




### 4.2 必建用例表（草案 ID；目录名可在 P1 定稿微调）



#### 探针 — `ascendc-tests/ml-kem/ml-kem-512/`


| ID            | P   | W   | 建议目录                                | 作用                             |
| ------------- | --- | --- | ----------------------------------- | ------------------------------ |
| **B1**        | P2  | W0  | `…-compress-decompress-du10-dv4-k2` | Compress/Decompress 10/4       |
| **B2**        | P2  | W0  | `…-byteencode-decode-d-k2`          | ByteEncode/Decode 10/4/12      |
| **B3a**       | P2  | W0  | `…-alg8-cbd-eta2-k2`                | CBD \eta=2 × k 维打包             |
| **B3b**       | P2  | W0  | `…-alg8-cbd-eta3-k2`                | **CBD \eta=3**（相对 768 的缺项）         |
| **B4**        | P2  | W1  | `…-alg7-sample-ntt-k2`              | SampleNTT i,j\in0,1            |
| **B5**        | P2  | W1  | `…-stage123-ntt-intt-polyvec4-k2`   | Stage1–3；**真 polyvec4 + 单核几何** |
| **B6**        | P2  | W1  | `…-alg11-12-multiply-inner-k2`      | MultiplyNTTs + Inner（k=2）      |
| **D13–D15**   | P2  | W2  | `…-alg13/14/15-…-k2`                | PKE device                     |
| **D19–D21ct** | P2  | W3  | `…-alg19/20/21/21ct-…-k2`           | KEM device + CT                |




#### incubating — `examples/incubating/ml-kem/ml-kem-512/`


| ID            | P   | W   | 建议目录                                             |
| ------------- | --- | --- | ------------------------------------------------ |
| **E13–E15**   | P3  | W4a | `exp-fips203-mlkem-pke-*-k2`                     |
| **E19–E21ct** | P3  | W4b | `exp-fips203-mlkem-kem-*-k2`（含 `…-decaps-ct-k2`） |


**明确不建**：1024/768 细切探针克隆；零垫；stable-512；frozen 抄码。

---



## 5. P2 — 波次节奏

每波固定：

```text
分析（形状/同步/I/O）→ 文书（INTEGRATION_PLAN 或 customspec）→ 实现 →
bash run.sh -r cpu -v Ascend910B4 →
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4 →
STATUS + 当日 qa 追加 +（能则）liboqs 交叉
```



### 5.1 依赖

```text
W0 (B1–B3b) ──► W1 (B4–B6) ──► W2 (D13–D15) ──► W3 (D19–D21ct)
                                                      │
                                                      ▼
                         W4a (E13–E15) ──► W4b (E19–E21ct) ──► glue
```



### 5.2 停问点


| P   | W    | 最高风险                        | 停问条件                       |
| --- | ---- | --------------------------- | -------------------------- |
| P2  | W0   | CBD‑3 定点 / LUT              | 与 liboqs 差 1 争议；无登记来源却重写内核 |
| P2  | W1   | **单核 mat_c / AIV 负载**       | 任何「先 pad」或「先改成多 cube」诱惑    |
| P2  | W2   | prep+compute+pack / session | launch 数要改参数卡              |
| P2  | W3   | FO / reject / 密文长 768       | `M_FILE`/`C_SRC` 歧义        |
| P3  | W4   | customspec 与 device 漂移      | I/O 不一致                    |
| P3  | glue | liboqs helper               | 交叉失败且非长度笔误                 |


---



## 6. P3 — glue 与完成定义



### 6.1 测试矩阵（建议）


| 对象            | CPU   | SIM_DIRECT | liboqs-512 交叉 | 备注                               |
| ------------- | ----- | ---------- | ------------- | -------------------------------- |
| B*            | ✓     | ✓          | 能则做           | B3b/B5/B6 必须有 ref                |
| D13–D15       | ✓     | ✓          | ✓             | D14↔D15 自 RT 建议做                 |
| D19–D21ct     | ✓     | ✓          | ✓             | reject≠accept                    |
| E*            | ✓     | ✓          | ✓             | 须 customspec                     |
| AscendC RT    | ✓×1   | ✓×1        | —             | KeyGen→Encaps→Decaps + reject 抽检 |
| liboqs KAT/RT | ✓×3 起 | ✓×1        | ✓             | helper 支持 512 后                  |




### 6.2 完成声明模板

> **有条件完成（至 incubating exp / ML-KEM-512）**：E13–E21ct CPU+SIM 绿；AscendC roundtrip 绿；liboqs-512 交叉 KAT（约定次数）绿；参数卡与单核决议一致；未做 stable / NPU。



### 6.3 性能口径（非超时门禁）

- 防挂死：各用例 `KERNEL_COMPUTE_BUDGET_SEC`（勿套用 NTT~15s 定标到全链）。  
- 「不错」：同算子 SIM Total tick **显著低于 768 对应 D/E**（期望量级随 k 下降）；无数量级回退；无标量 GM 扫全表。  
- 优化穿插在各波次内做，**不**另开无限性能专项挡完成声明。

---



## 7. 文档与索引义务


| 时机    | 动作                                                                                         |
| ----- | ------------------------------------------------------------------------------------------ |
| P0    | 更新 `ascendc-tests/ml-kem/INDEX.md`、`examples/incubating/ml-kem/INDEX.md`；**不**建 stable-512 |
| 每波    | `STATUS.md`；当日 `qa/` 追加；tick → `qa/active_sim_regress_summary.md`                          |
| 登记表缺块 | **停**                                                                                      |
| 收工    | `AGENT_HANDOFF`、参数卡退出清单、本计划状态改「有条件完成」                                                      |


---



## 8. 建议排期（技术依赖，非日历）


| 序   | P   | W       | 内容                              | 退出            |
| --- | --- | ------- | ------------------------------- | ------------- |
| 1   | —   | —       | 用户审阅本计划 + 锁 §3.2 / §9           | 书面 OK         |
| 2   | P0  | —       | 参数卡定稿、目录壳、sizes、helper          | 参数锁定          |
| 3   | P1  | —       | 缺项对照（补缺图）+ 必建表定稿                      | 表锁            |
| 4   | P2  | W0      | B1–B3b                          | 积木双绿（含 CBD‑3） |
| 5   | P2  | W1      | B4–B6                           | 单核 NTT/内积双绿   |
| 6   | P2  | W2      | D13–D15                         | PKE device    |
| 7   | P2  | W3      | D19–D21ct                       | KEM device    |
| 8   | P3  | W4+glue | exp + RT + liboqs KAT；刷 HANDOFF | 有条件完成         |


---



## 9. 决议记录（2026-07-27 已确认）

> 用户已全部打钩确认。

- [x] **#1 单 AI Core** — **S-1**（单 cube，`blockDim=1`；cube 内可用 2 AIV；禁跨 cube 切 poly）
- [x] **#2 噪声批** — **T-B2** polyvec4；\(\hat A\) 独立 prep；禁零垫
- [x] **#3 KEM device** — **保留** D19–D21
- [x] **#4 reject / CT** — **要求** D21ct + E21ct
- [x] **#5 PKE exp** — **要做** E13–E15
- [x] **#6 命名** — **`-k2`**
- [x] **#7 liboqs-512 交叉** — **本阶段必达**（含修 helper）
- [x] **#8 stable-512** — **本阶段不建**
- [x] **#9 自主授权** — Agent **自主推进至有条件完成**；仅计划 §0.1 停问点打断

| # | 议题 | 已锁 |
|---|------|------|
| 1 | 单 AI Core | S-1 |
| 2 | 噪声批 | T-B2 polyvec4 |
| 3–5 | 范围 | D19–D21 + CT + PKE exp |
| 6–8 | 命名 / KAT / stable | `-k2` · liboqs 必达 · 不建 stable |
| 9 | 推进方式 | 自主（有停问点） |

**下一刀**：Agent 落 P0 目录壳 / sizes / helper → P1 定稿 → 开 W0（无需再等授权）。

---



## 10. 参考路径（只读）

- 768 计划 / 参数卡 / P1 表（P/W 分列约定）  
- 教材第8章（768 复盘）：方法论闭环范本  
- NTT / Compress / CBD‑η2 / KEM notes（`docs/notes/`）  
- liboqs：`OQS_KEM_ml_kem_512_*`  
- **frozen**：可进读 `FROZEN.md`，**出门不带码**

