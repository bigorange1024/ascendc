# frozen — 已冻结 / 废弃的 examples 实验

**前缀**：`frozen-exp-<简述>/`（从 `incubating/` 迁出）

**语义**：**路线关闭判决**。可**进入**阅读关闭说明；**禁止**将源码/路线/customspec **带出**到活跃 `exp-*`。

| Agent / 开发者 | 规则 |
|----------------|------|
| **可读（进门）** | `FROZEN.md`、`STATUS.md`、本 `INDEX.md`、`qa/` 纪要 |
| **禁止（出门）** | 将 frozen-exp 源码/路线/customspec **带入**活跃 `exp-*` / `stable-*` |

细则见 [.cursor/rules/ascendc-development.mdc](../../.cursor/rules/ascendc-development.mdc) §`**/frozen/`**；[研究路线与frozen治理.md](../../docs/notes/研究路线与frozen治理.md)。探针侧 frozen 见 [ascendc-tests/frozen/INDEX.md](../../ascendc-tests/frozen/INDEX.md)。

**冻结日期**：2026-06-11（stage12-mix 自 2026-06-10 起已标废弃）  
**共同背景**：在 F203 / Kyber NTT 中用 `Matmul<>` 高阶 API（融合 MIX 或纯 AIC Stage2），而非 `merged_kyber` FSM + `AicMmad`。

经验教训：[qa/2026-06/2026-06-11-…#NTT-Matmul](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)

**替代路线**（**勿抄本 frozen 目录**）：

- `examples/incubating/exp-sepolyvec8-ntt-k8/` — **纯 $k{=}8$** 批 NTT 回归对照（交错 S0；**非** KeyGen 集成）
- `ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/` — Alg.13 行 16–20 向量全链路
- `examples/incubating/exp-mlkem-f203-alg13-16171820-2s1e-k4/` — 同上【预研】

---

## 2026-06-19 — 块紧凑 S0 路线否决

| 目录 | 原角色 | 冻结原因（摘要） |
|------|--------|------------------|
| [frozen-exp-mlkem-sepolyvec8-ntt-k4-block/](frozen-exp-mlkem-sepolyvec8-ntt-k4-block/) | Alg.13 行 16–17 块紧凑批 NTT exp | **A2 1:2 MIX** 下 Stage3 GM Gather 分散；ntt_study 1:1 结论不可迁移；仅 customspec、未实现 |

详见 `FROZEN.md`；探针侧同期否决见 [ascendc-tests/frozen/INDEX.md](../../ascendc-tests/frozen/INDEX.md) §2026-06-19；纪要 [qa/2026-06/2026-06-19-… §10](../../qa/2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md#10-块紧凑-s0-路线否决)。

---

## 用例与冻结原因

### [frozen-exp-mlkem-f203-stage12-encode-matmul-mix/](frozen-exp-mlkem-f203-stage12-encode-matmul-mix/)

| 项 | 内容 |
|----|------|
| **原路径** | `examples/incubating/exp-mlkem-f203-stage12-encode-matmul-mix` |
| **CPU** | 部分（需两趟 launch 绕过 CrossCore 死锁） |
| **SIM** | ✗ encode 写 GM 失败 |
| **冻结类型** | **废弃**（2026-06-10 首次标废弃，2026-06-11 迁入 `frozen/`） |

**为何冻结**：

1. 采用 CANN **LeakyRelu 融合 MIX 模板**（`Matmul<>` + auto_gen），企图单 kernel 覆盖 Stage1 encode + Stage2 matmul。
2. AIV→AIC→AIV 阶段边界被模板封装，**workspace / Iterate / CrossCore 不透明**；CPU 无法忠实代表 SIM 同步语义。
3. SIM 上 encode 阶段写 GM 即失败；与 `merged_kyber` 手写 FSM（poly8-s123 CPU+SIM ✓）对比后放弃。
4. customspec PDF **勿再**作实现依据。

---

### [frozen-exp-mlkem-f203-stage2-int8-matmul-cube/](frozen-exp-mlkem-f203-stage2-int8-matmul-cube/)

| 项 | 内容 |
|----|------|
| **原路径** | `examples/incubating/exp-mlkem-f203-stage2-int8-matmul-cube` |
| **CPU** | ✓ `aicore=1` / `aicore=4` 对拍 |
| **SIM** | 未纳入验收 |
| **冻结类型** | **路线废弃中止**（2026-06-11） |

**为何冻结**：

1. 纯 AIC 隔离探针：`Matmul<>` 实现 F203 Stage2（`[16,256]×[256,512]`，LUT 在右），与 Alg13 宽表语义对齐。
2. 与 limb6 批 NTT 的紧凑左矩阵 `[2k,256]` / `AicMmad` **不是同一 GM 契约**；仅适合历史 golden 对照。
3. NTT 全链路拍板改用 `AicMmad`；本实验未跑 SIM，继续维护无交付价值。
4. 多核 tiling 扫参已迁至 `ascendc-tests/frozen/frozen-int8-matmul-cube-*`（同属废弃路线）。
