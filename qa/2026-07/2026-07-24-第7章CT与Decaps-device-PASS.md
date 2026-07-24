# 2026-07-24 — 第7章 CT → Decaps device PASS · `#交付#` · 非 NPU 压测

关键字：`CT_decaps` · **T19b/c** · **`#交付#`** · **`-ct` 改名**避 main · **拒绝 SIM** · KAT **10+3** · **roundtrip** · `M_FILE` · 未跑 NPU

## 决策

1. 按教材第7章已提交前瞻闭包表 `CT_decaps` 实现 Decaps device；落点目录更名为 `pass-fix-…`（废 stub `fix-…-decaps-device-k4`）。
2. **Forbidden**：不抄 `fix-*-correctness-*` / `frozen/` 的 `.cpp/.hpp`；correctness 只读契约。
3. PKE：**编译期引用** stable Decrypt fused + Encrypt；SIM 用 `prepare_dec_shim.sh` 做 `dec_*` 头隔离合单库（不改 stable 源码）。
4. KEM 头/尾：对齐 Encaps 范式 — `G` 并入 Phase-E prep；FO 在 pack/`l18_l19` 尾；`Sha3OneShot`/`Shake256OneShot`。
5. SIM 生产默认 **`ASCENDC_SIM_HOST_MODE=decaps_2session`**（CT 锁定；与 main 上 1-session 定论并存为排障对照）。

## 证据

| 项 | 结果 |
|----|------|
| CPU 合法 | `[verify] PASS` / `K` max=0 |
| SIM 合法 | `[verify] PASS`；tick D**286798**+E**763663**；根无 stray |
| 拒绝 CPU | `KEM_DECAPS_REJECT=1` → `REJECT PASS` |

## 遗留（午前）

- 教材 §实现后判决占位待改写（弱/强成功：合法+拒绝已绿；结构无 vendor）
- 拒绝路径 SIM 长测可选；KAT×10 非本轮门禁
- 未开 `examples/` exp/stable Decaps（午前禁止）

---

## `#交付#` Decaps → examples（同日追加）

用户授权完整晋级：

1. **incubating** [`exp-fips203-mlkem-kem-decaps-ct-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-ct-k4/)：vendored Decrypt+Encrypt + `kem/`；customspec + [`kem-decaps-baseline-registry`](../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md)。
2. **CPU+SIM** 合法路径 `K` max=0（exp：D**286829**+E**763658**）；拒绝 CPU PASS。
3. **整树复制** → [`stable-fips203-mlkem-kem-decaps-ct-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4/)（v1）；本目录复验 CPU+SIM（D**286866**+E**763780**）。
4. `scripts/` `DECAPS_DIR` 默认改指 stable（对照 Encaps T19e）；device 探针仍为行为基线。

**Forbidden 未触**：未抄 correctness/frozen 源码。

---

## 非 NPU 压测收尾（同日追加）

用户：「除 NPU 外，其余测试继续完成」。

### 脚本修复（须先合入再批测）

| 问题 | 修复 |
|------|------|
| CPU twin Phase-E 读 `input/golden_v.bin`，缺匹配 `m` → FO 误走拒绝 | KAT / roundtrip 传 **`M_FILE`**（encaps stash `m.bin`） |
| `run.sh` verify 失败仍打印 SUCCESS | `python3 verify… \|\| exit $?`（device/exp/stable） |
| roundtrip Phase 4 旧 `TAMPER_C` 注释 | 对齐 **Gate E3** `KEM_DECAPS_REJECT=1` |

### 证据（Cloud；勿并行多路 SIM）

| 项 | 结果 |
|----|------|
| stable / exp / device **拒绝 SIM** | `REJECT PASS`；根无 stray；tick≈D**286k**+E**763k** |
| `liboqs_kem_decaps_batch` | **CPU×10** + **SIM×3** **PASS** |
| `roundtrip_kem_keygen_encaps_decaps` | **CPU + SIM** **PASS**（agreement + E3 reject） |
| **NPU** | **未跑**（本环境无卡） |

---

## 五指标对照表（同日追加）

精简表已单列：[`docs/research/2026-07-24-Decaps-correctness与CT五指标对照.md`](../../docs/research/2026-07-24-Decaps-correctness与CT五指标对照.md)（A correctness vs B CT；二元 1+2+4+5 → A**=1** / B**=4**；假绿 A**≥3** / B**=3**）。

---

## 目录改名 `-ct`（同日追加）

与 **main** 上已交付的同名三树区分：本专题分支保留第7章 CT 实验副本，统一加后缀 **`-ct`**：

| 旧 | 新 |
|----|----|
| `pass-fix-f203-alg21-kem-decaps-device-k4` | `pass-fix-f203-alg21-kem-decaps-device-ct-k4` |
| `exp-fips203-mlkem-kem-decaps-k4` | `exp-fips203-mlkem-kem-decaps-ct-k4` |
| `stable-fips203-mlkem-kem-decaps-k4` | `stable-fips203-mlkem-kem-decaps-ct-k4` |

约定：后续只在 **`research/formal-lang-dag`** 推进；**勿**再新建旁支。
