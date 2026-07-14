# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-14（家里：**KEM KeyGen incubating【预研】有条件完成并 push**；办公室下一项见下）

---

## ★ 强制写法（2026-07-08，新代码均须遵守）

| 类型 | 写法 | 禁止 |
|------|------|------|
| CPU vs SIM | `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| SIM host 拓扑 | `ASCENDC_SIM_HOST_MODE` + §3.3 登记表 | `F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等 |
| 算法变体 | CMake 宏（CPU/SIM 同值） | env 切 launch |

详文：[`AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) §4.1 · [`library/shared/INDEX.md`](library/shared/INDEX.md)

**无 NPU 时验收权重**：全链 PKE — **SIM = 主参考**；**CPU = 辅助正确性**。

**Fail 复现纪律**：偶发 FAIL 必须落盘 `mode` + `kem_seed` hex（或 `SEED_D`）+ 错位偏移 + 是否清零 `output/`（`kat_liboqs_kem_keygen.py` 已写 seed；压测脚本同理）。

---

## ★ 当前真相（2026-07-14 · 家里 push）

### PKE 三段 — **stable 交付齐备**（未改）

| 段 | stable | SIM tick（参考） |
|----|--------|------------------|
| KeyGen | [`stable-fips203-mlkem-pke-keygen-k4`](examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | ~542k |
| Encrypt | [`stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | ~627k |
| Decrypt | [`stable-fips203-mlkem-pke-decrypt-k4`](examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | ~283k |

### KEM Alg.19 KeyGen — **incubating 有条件完成（已入库）**

| 路径 | 角色 |
|------|------|
| [`exp-fips203-mlkem-kem-keygen-k4/`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) | **自包含**实现 + customspec；见 [`STATUS`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/STATUS.md) |
| [`pass-fix-f203-alg19-kem-keygen-device-k4`](ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) | 行为对照基线（~713k）；**禁止** CMake 依赖本树 |
| `examples/stable/stable-fips203-mlkem-kem-keygen-k4/` | **尚无**；须用户明确 `#交付#` 后从 incubating **复制晋级** |
| registry | [`docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md`](docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md) |

**家里验收（已绿）**：CPU×40（清零 output）· SIM tick **707057** · vs correctness×10 · liboqs CPU×10 · **liboqs SIM×3**。

**踩坑落地**：SIM `SyncAll`→AIV0 尾；CPU soft-flag + AIV1 尾；`KYBER_PIPE_ALL` 恒 barrier；VERIFY 前清零 output。

纪要：[`qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md`](qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md)

---

## ★ 办公室 Agent 下一任务（按优先级）

### P0-1 — 等用户口令后再做：`#交付#` KEM KeyGen → stable

**仅当用户明确说 `#交付#` / `#验收#` 时**：

1. 从 [`exp-fips203-mlkem-kem-keygen-k4`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) **复制晋级**为 `examples/stable/stable-fips203-mlkem-kem-keygen-k4/`（禁从零重写、禁依赖 device）。
2. 双模式再验收：`bash run.sh -r cpu` + `SIM_DIRECT=1 bash run.sh -r sim`；建议再跑 liboqs CPU×N + SIM×≥1。
3. 更新 `examples/stable/INDEX.md`、`qa/TODO.md`（关 T19f 交付段）、当日 `qa/`、本文件。

**未获口令前禁止建 stable。**

### P0-2 — 主线开工：**T19a Encaps device**

[`fix-f203-alg20-kem-encaps-device-k4`](ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/)：改接 [`stable-…-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)（或 pass-fix Encrypt device）布局；CPU+SIM `c`/`K` max=0；分项 kat。  
随后 T19b/c Decaps Phase-E/D。

### P1 — **T21** SHA3hp

待用户拍板是否替换设备 SHA3-256/512；未拍板不改默认路径。

### 纪律提醒（相对 07-13 失败）

- **禁止**未压测绿就晋级 stable。
- **禁止**从 `frozen/` / 已删树抄实现。
- FAIL 必须落盘 seed；勿只写「偶发 dk FAIL」。

---

## 验收命令（smoke）

```bash
git pull

# KEM KeyGen incubating
cd examples/incubating/exp-fips203-mlkem-kem-keygen-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# liboqs（可按需）
KEYGEN_DIR="$(pwd)" KEM_KG_CPU_TRIALS=3 KEM_KG_SIM_TRIALS=1 SIM_DIRECT=1 \
  python3 ../../../scripts/kat_liboqs_kem_keygen.py
```

**WSL**：`CMAKE_BUILD_JOBS=2`；勿并行多 SIM。

---

## 附录：关键路径速查

| 主题 | 路径 |
|------|------|
| KEM KeyGen incubating | [`exp-…-kem-keygen-k4/`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) |
| customspec | [`…-实现方案-customspec.pdf`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf) |
| device 基线 | [`pass-fix-f203-alg19-kem-keygen-device-k4/`](ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) |
| 家里当日纪要 | [`qa/2026-07/2026-07-14-….md`](qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
