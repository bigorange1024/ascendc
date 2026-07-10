# INTEGRATION_PLAN — pass-fix-f203-alg14-pke-encrypt-device-k4

**定位**：Alg.14 **完整 K-PKE.Encrypt（FIPS 行 1–22）** — 将两上游 **PASS** 探针在 **单 device session、GM handoff** 下串联；输入 `ek_pke ‖ m ‖ coins`，输出 **仅** `c = c₁ ‖ c₂`（1568B）。行 1 `N←0` 无运算；行 2 `t̂←ByteDecode₁₂(ek)` 在 compute 核内完成。

**符号**：FIPS 203 Algorithm 14（ml_kem_1024 / k=4）。

**状态**：**CPU + SIM PASS**（2026-07-08）；`c.bin` max=0（两模式），SIM tick **626121**，2 launch，根目录 0 stray dump。见 [`STATUS.md`](STATUS.md)。

**种子（已锁定）**：`SEED_D=20260619` — 全链唯一；`input/` 与 `golden/c.bin` 复制自 correctness 探针（§4.1、§8）。

---

## 1. 数学全链（锁定）

```text
# ── prep（行 3–15）──
ρ ← ek_pke[1536:1568]
∀ (p,j): a_hat[p,j] ← SampleNTT(ρ || byte(j) || byte(p))
∀ n∈[0,8]: poly_n ← SamplePolyCBD_η=2(PRF(coins, n))
r[0..3] ← poly_0..3；e₁[0..3] ← poly_4..7；e₂ ← poly_8

# ── compute+tail（行 2/16–24）──
t̂ ← ByteDecode₁₂(ek_pke)
ŷ ← NTT(r)                    # 符号 y ≡ r
(û, tr̂) ← (Âᵀ | t̂ᵀ) ∘ ŷ
u ← INTT(û) + e₁
e₂' ← e₂ + μ(m)  (mod q)      # 融合核前缀，无单独 μ launch
v ← INTT(tr̂) + e₂'
c₁ ← ByteEncode₁₁(Compress₁₁(u))
c₂ ← ByteEncode₅(Compress₅(v))
c ← c₁ ‖ c₂
```

**与旧 G5 探针差异**：本路线 **不复刻** `stable-fips203-mlkem-pke-encrypt-k4` 的多段 AIV/MIX 拼装；仅 **vendoring** 两上游 pass 探针已验收内核。

---

## 2. 边界

### 2.1 本目录做

| 项 | 说明 |
|----|------|
| 统一 GM arena + host `main.cpp` 编排 | 两 launch（SIM）/ 五 launch（CPU） |
| vendoring | prep 树 + compute+tail 树 → 本目录 `prep/`、`compute/` |
| `f203_encrypt_full_layout.h` | 全链 GM 偏移契约 |
| `scripts/gen_data.py` | 自包含 golden：优先复用 correctness；缺失则 `SEED_D` 本地生成 → `golden/c.bin` |
| Gate S1–PASS | 见 §7 |

### 2.2 本阶段不做

| 项 | 说明 |
|----|------|
| prep 与 compute **单 kernel 融合** | prep=AIV blockDim=2，compute=MIX blockDim=1；FSM 合并成本过高 |
| SIM **1 launch** 全链 | 保留为远期优化；先 **2 launch** GM 验收 |
| 晋级 `examples/stable/` | 本探针 PASS 后再议 |
| KEM Encaps 并入 | 仍由 [`fix-f203-alg20-kem-encaps-correctness-k4`](../fix-f203-alg20-kem-encaps-correctness-k4/) 负责 |

---

## 3. 上游探针与抄码来源

| 段 | 上游目录 | 核心 kernel | 抄入本目录 |
|----|----------|-------------|------------|
| prep | [`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](../pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) | `f203_encrypt_prep` | `prep/` 全树 + `f203_encrypt_prep_{layout,entry,ub}.cpp` |
| compute+tail | [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) | `f203_encrypt_l18_l19` | `compute/` 全树 + layout/tiling 头 |

**抄码规则**（与仓库通则一致）：

- **允许**：复制 `.hpp/.cpp/.h` 到本目录；`library/shared/`、`scripts/` CANN 壳
- **禁止**：运行时 `#include` 其它探针路径；从 `frozen/` 或 G5 correctness 探针抄码

prep vendoring 仍走 `scripts/vendor_sync_from_stable_keygen.sh`（来源 stable KeyGen prep，与 prep 探针一致）。

---

## 4. I/O 契约

### 4.1 Host 输入（**锁死种子 + 复用现成 fixtures**）

**唯一种子** `SEED_D=20260619`，整套输入 **直接复制自** [`stable-fips203-mlkem-pke-encrypt-k4/input/`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/input/)（该套已经过 device 全链验证 max=0）：

| 路径 | 尺寸 | 来源（复制，非重生成） |
|------|------|------------------------|
| `input/ek_pke.bin` | 1568B | correctness `input/ek_pke.bin`（ρ 在 `[1536:1568]`） |
| `input/m.bin` | 32B | correctness `input/m.bin`（`default_rng(SEED_D+991)`） |
| `input/coins.bin` | 32B | correctness `input/coins.bin`（同 rng 续取） |
| `input/lut_*.bin` | 4×64KB | correctness LUT（NTT/INTT even/odd） |

**关键**：全链只有一套种子；prep（ρ→a_hat、coins→r/e₁/e₂）与 compute+tail（ek→t̂、m→μ）**共用**上表。**不再**沿用 prep 探针的 `COINS_SEED=20260706` 或 compute+tail 探针的 `20260708`——那两套仅用于各自单段回归，全链一律 `SEED_D=20260619`。

`scripts/gen_data.py`：**优先**从 correctness 目录复制 input/golden_c；**缺失时本目录自生成**（`gen_ek_pke(SEED_D)` + `rng(SEED_D+991)`→m/coins + `golden_encrypt`→c），探针可独立跑通。

### 4.2 Host 输出（对齐 FIPS 203 Alg.14：唯一输出密文 c）

| 路径 | 尺寸 | 说明 |
|------|------|------|
| `output/c.bin` | **1568B** | **唯一产物**（= Alg.14 输出 c = c₁‖c₂） |

- u/v 为设备内部中间量，**不 D2H、不落盘**（Alg.14 输出定义只有 c）。
- `input/golden_v.bin`（1024B）：**CPU 分段实现的注入 golden**（CPU 三 launch 无 k=8 INTT 不产 v），非产物；SIM 全设备不需要。

### 4.3 设备 GM handoff（单 arena，禁止 D2H 中转）

```
┌─────────────────────────────────────────────────────────────┐
│  INPUTS（host H2D 一次）                                     │
│  ek_pke[1568]  m[32]  coins[32]                             │
├─────────────────────────────────────────────────────────────┤
│  Launch 1 WRITES / Launch 2 READS                          │
│  a_hat[16,256] int32     ← prep                             │
│  re[9,256] int32         ← prep（r‖e₁‖e₂ 扁平）              │
├─────────────────────────────────────────────────────────────┤
│  Launch 2 WRITES                                             │
│  u[4,256] v[256]（设备内部，不 D2H）  c[1568]（唯一 D2H 产物）│
├─────────────────────────────────────────────────────────────┤
│  SCRATCH                                                     │
│  ws_compute[331776]      ← compute+tail tiling::wssize       │
│  prep 内部 UB/GM scratch   ← prep 核自持（见 prep layout）    │
└─────────────────────────────────────────────────────────────┘
```

### 4.4 `re` → compute 指针映射（**零拷贝**）

prep 输出 `re` 布局（与 prep 探针 `golden_re.bin` 一致）：

| poly 下标 | 语义 | byte 偏移（int32 系数） |
|-----------|------|-------------------------|
| 0–3 | **r**（≡ Alg.14 的 **y**） | `0 .. 4×256-1` |
| 4–7 | **e₁** | `4×256 .. 8×256-1` |
| 8 | **e₂** | `8×256 .. 9×256-1` |

Launch 2 传参（**同 GM 基址 + 偏移**）：

```cpp
int32_t *reGm = ...;
int32_t *yGm  = reGm + 0 * N;      // r
int32_t *e1Gm = reGm + 4 * N;
int32_t *e2Gm = reGm + 8 * N;
```

**禁止** host 将 `re` D2H 再拆成三个 buffer H2D。

---

## 5. Launch 拓扑

### 5.1 SIM（生产路径）

```text
Session 1 aclrtStream
  Launch 1  f203_encrypt_prep     blockDim=2  AIV_ONLY
            in:  ek_pke, coins
            out: a_hat, re
            tick ≈ 470502

  Launch 2  f203_encrypt_l18_l19   blockDim=1  MIX_AIC_1_2
            in:  ek_pke, m, a_hat, y/e1/e2(re 切片), ws+LUT
            out: u, v, c（内联 tail pack）
            tick ≈ 154781

合计 2 launch，tick ≈ 625k
```

Launch 2 前 host 仅：

1. H2D `ek_pke`, `m`, `coins`（Launch 1 已用 ek/coins）
2. H2D 或预置 `ws` 内 NTT/INTT LUT（与 compute+tail 探针相同）
3. **不** D2H `a_hat`/`re`

### 5.2 CPU tikicpu

| # | kernel | 说明 |
|---|--------|------|
| 1 | `f203_encrypt_prep` | 产 `a_hat`, `re` |
| 2 | `f203_encrypt_ntt_y` | MIX 三 launch 段 1 |
| 3 | `f203_encrypt_at_jp` | 段 2 |
| 4 | `f203_encrypt_intt_e1` | 段 3 → **u** |
| 5 | `f203_encrypt_alg14_pack` | **v=golden**；pack → **c** |

与 compute+tail 探针 CPU 路径一致，仅在最前加 prep launch。

---

## 6. 文件结构

```text
pass-fix-f203-alg14-pke-encrypt-device-k4/
├── INTEGRATION_PLAN.md
├── STATUS.md
├── f203_encrypt_full_layout.h   # prep↔compute GM handoff 契约
├── main.cpp                     # SIM 2 / CPU 5 launch 编排
├── f203_encrypt_tiling.cpp
├── CMakeLists.txt
├── run.sh                       # 默认 SKIP_REBUILD + SIM_DIRECT=1 + CMAKE_BUILD_JOBS=2
├── prep/                        # vendored（scripts/vendor_sync_from_stable_keygen.sh）
├── compute/                     # vendored 自 compute+tail 探针
└── scripts/
    ├── gen_data.py              # 自包含：优先 correctness，缺失则本地 SEED_D 生成
    ├── host_golden/             # gen_ek_pke / golden_c / f203_ref_common
    └── vendor_sync_from_stable_keygen.sh
```

---

## 7. 分期 Gate（**CPU+SIM 视为一件事**）

> **验收原则**：一个探针的「通过」= **同一套输入下 CPU 与 SIM 都对拍 max=0**。不把 CPU、SIM 拆成两个独立里程碑；任一模式未过即整体未过。

| Gate | 内容 | 验收（**CPU ∧ SIM 同时满足**） | 状态 |
|------|------|-------------------------------|------|
| **S0** | 本方案 + 目录 + vendoring 脚本 | 用户确认方案 | ✓ |
| **S1** | host arena + 2 launch（SIM）/ 5 launch（CPU）接线 | `bash run.sh -r cpu` **且** `bash run.sh -r sim` → `c.bin` max=0 | ✓ **CPU max=0 / SIM max=0 (tick 626139)**；输出仅 c |
| **PASS** | 已重命名 `pass-fix-f203-alg14-pke-encrypt-device-k4` | 同上双模式绿灯 + SIM 根目录 0 stray dump | ✓ 已晋级（2026-07-08）；输出仅 c |

**可选加验**（非阻塞，不单列 gate）：中间量 `a_hat`/`u`/`v` debug 对拍；liboqs L2 交叉（§8.3）。

---

## 8. Golden（**锁死种子 · 复用现成输出**）

全链只认 `SEED_D=20260619` 一套输入（§4.1），期望密文 `c` 有**三条互证的现成来源**，任选其一即满足「I/O 等价」：

### 8.1 首选：直接复用 correctness 的 `golden_c.bin`

[`stable-fips203-mlkem-pke-encrypt-k4/output/golden_c.bin`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/output/golden_c.bin)（1568B）由该探针自包含 python `scripts/host_golden/golden_c.py(ek,m,coins)` 生成，且其 device 输出 `fixtures/c.bin` 已与之逐字节一致（max=0）。

```text
gen_data.py（本探针，自包含）:
  1. 若 correctness/input/{ek_pke,m,coins} 齐全 → 复制；否则本地 gen_ek_pke + rng
  2. 本地生成 lut_ntt/intt_*_stacked.bin
  3. 若 correctness/output/golden_c.bin 存在且输入来自复制 → 复制并与本地 golden_encrypt 对拍；
     否则本地 golden_encrypt → golden/c.bin
  4. 写 input/golden_v.bin（CPU 注入，非产物）
```

### 8.2 备选：本地 python 重算

调 correctness 的 `scripts/host_golden/golden_c.py`（同 `SEED_D=20260619`）重算，结果应与 8.1 逐字节一致（可作为一致性自检）。

### 8.3 备选：liboqs 交叉

[`scripts/liboqs_pke_vs_ascendc.sh`](../../scripts/liboqs_pke_vs_ascendc.sh) 的 Encrypt 段（`SEED_D=20260619`，`Compress_5` 舍入修复后 max=0）。

**三源一致性**（correctness device c == python golden_c == liboqs c）此前已确立；本探针只需产出的 `c.bin` 对齐其中任一即可，**禁止**把 correctness 的实现源码当规格（仅用其 I/O）。

### 8.4 种子锁定表

| 变量 | 锁定值 | 用途 |
|------|--------|------|
| `SEED_D` | **20260619** | 唯一种子；ek/m/coins 全部派生 |
| `input/*.bin` | 复制自 correctness | ek_pke / m / coins / lut |
| `golden/c.bin` | 复制自 correctness `output/golden_c.bin` | 期望密文 |

---

## 9. 关键不变量（审查红线）

| # | 不变量 |
|---|--------|
| R1 | INTT 输入来自内积 **当次 UB**（`ProcessFromLocal`），见 [F203-Encrypt-compute-行18-19-UB驻留技术总结.md](../../docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md) |
| R2 | `e₂+=μ` 在融合核 **INTT 前**完成；tail **纯 pack** |
| R3 | tail：**Compress 向量 + ByteEncode 标量 pack**（不激活 `BYTE_ENCODE_D_VEC=2`） |
| R4 | prep→compute handoff **GM 同址**；`y≡r` 仅符号差异，布局用 prep `re` 前 4 poly |
| R5 | poly-batch：Stage2 后每个 AIV 握有完整 poly 的 hi+lo（ML-KEM NTT 通则） |
| R6 | 自包含：禁止跨探针 `#include`；禁止 Host 辅助密码计算（除 golden） |

---

## 10. 性能与资源

| 项 | 估计 |
|----|------|
| SIM tick | ~**625k**（470502 + 154781） |
| 对比 G5 correctness | 922441 tick → 本路线约 **−32%** |
| `KERNEL_COMPUTE_BUDGET_SEC` | 建议 **900**（prep+compute 段；写进 `run.sh` 头注释） |
| blockDim | prep **2** + compute **1**（不改为 1+1 合并） |

---

## 11. 与旧 G5 / stable 关系

| 路径 | 关系 |
|------|------|
| [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | **golden oracle**：复用其 `input/`（ek/m/coins/lut）+ `output/golden_c.bin`（黑盒 I/O，SEED_D=20260619）；**禁止**抄其 kernel/prep/pack 实现源码 |
| [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) | compute+tail **唯一**抄码来源 |
| [`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](../pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/) | prep **唯一**抄码来源 |
| `examples/stable/stable-fips203-mlkem-pke-encrypt-k4` | **未建**；本探针 PASS 后可复制晋级 |

---

## 12. 参考文档

| 文档 | 内容 |
|------|------|
| [F203-Alg14-Encrypt-compute-tail-PASS技术总结.md](../../docs/notes/F203-Alg14-Encrypt-compute-tail-PASS技术总结.md) | compute+tail 模式 P-ECT-1/2/3 |
| [F203-ByteEncode-ByteDecode-d-向量与标量选型.md](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md) | tail 算子宏 |
| [用例自包含与设备全链约束.md](../../docs/engineering/用例自包含与设备全链约束.md) | 自包含红线 |

---

## 13. 验收命令（实现后 · CPU+SIM 一并）

```bash
cd ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4
# prep 树已 vendored；可选刷新：bash scripts/vendor_sync_from_stable_keygen.sh
bash run.sh -r cpu -v Ascend910B4                 # 默认 SKIP_REBUILD；SEED_D=20260619 自包含 golden
bash run.sh -r sim -v Ascend910B4                 # 默认 CAModel 金标；无需手动 SIM_DIRECT
# 强制重编：ENCRYPT_FORCE_REBUILD=1 bash run.sh -r cpu -v Ascend910B4
```

**成功判据（两条都要绿）**：CPU 与 SIM 均 `[SUCCESS] …` 且 `c.bin` vs `golden/c.bin` **max_abs_diff=0**；SIM 根目录 **0 stray dump**。任一模式失败即整体未通过（不接受「先记 CPU 通过、SIM 待办」的拆分）。
