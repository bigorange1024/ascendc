# FIPS 203 ML-KEM-1024 PKE KeyGen — baseline-registry

**主题**：Alg.13 K-PKE.KeyGen（k=4）交付侧 golden / KAT 计算块登记  
**适用**：交付以 **`examples/stable/stable-fips203-mlkem-pke-keygen-k4`** 为准（2026-06-29 自 incubating 复制晋级）；预研副本 `examples/incubating/exp-fips203-mlkem-pke-keygen-k4` 保留。  
**补登记**：2026-07-20（历史晋级时缺表；按现网 `scripts/gen_data.py` / `keygen_golden.py` 回溯）。  
**规则**：生成 `input/` / `golden_*` 的计算内核**仅**可调用下表已验证来源；缺项须停下补登记。

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `seed_d.bin` | 4 B | `uint32` LE；默认 host SHA3（`fips203_host_rng`）；`SEED_D=` 可覆盖定点 |
| 输入 | `lut_even/odd_stacked.bin` | 静态 | NTT limb LUT（与 seed 无关） |
| 输出 / golden | `ek_pke.bin` | 1568 B | `ByteEncode₁₂(t̂)‖ρ` |
| 输出 / golden | `dk_pke.bin` | 1536 B | `ByteEncode₁₂(ŝ)` |

中间 Â / ŝ / ê / ρ 等 **禁止**作为交付 I/O 落盘（调试 dump 标非默认）。

---

## 2. 已验证计算块

| 计算块 | 用途 | 已验证来源 | 备注 |
|--------|------|------------|------|
| `resolve_seed_d` / 派生 | Host 定点或 SHA3 种子 | `library/shared/fips203_host_rng` | 默认生产种子策略 |
| `build_full_keygen` → ek/dk | golden `ek_pke`/`dk_pke` | 本目录 `scripts/keygen_golden.py`（编排 prep/compute 子 golden） | **仅** Host oracle；非 AscendC 规格 |
| SampleNTT / CBD / NTT / hat / ByteEncode₁₂（host） | golden 子块 | 本目录 `scripts/prep/`、`scripts/compute/` + `f203_ref_common` / LUT | 禁止在 golden 路径另写 NTT 核心 |
| 静态 NTT LUT | `gen_data` | `load_lut_t_i8`（同 Encrypt 族） | 只读 |
| liboqs indcpa_keypair 等 | **KAT 对照 oracle** | `scripts/liboqs_pke_ref` / 用例 `kat_liboqs_vs_ascendc.sh` | **仅** KAT；非实现规格 |
| KeyGen 主体（设备） | Alg.13 | AscendC；I/O 对拍即可 | **禁止**「与 keygen_golden 同构」验收 |

---

## 3. 明确禁止

- 在 golden 路径重写 NTT / CBD / ByteEncode 核心公式。
- 把 liboqs / 参考 C 逐步移植进 AscendC 并以源码同构验收。
- Host 参与**默认生产**写 `output/`（VERIFY/KAT 除外）。

---

## 4. 验收脚本（交付门禁）

| 脚本 | 默认 | 对拍对象 |
|------|------|----------|
| `bash run.sh -r cpu\|sim -v Ascend910B4` | 全量 | `ek_pke`/`dk_pke` vs golden |
| `bash kat_liboqs_vs_ascendc.sh` | CPU×10 + SIM×1 | AscendC vs liboqs |

---

## 维护

变更 golden 计算来源或新增计算块 → 同步本表与 [`docs/specs/INDEX.md`](INDEX.md)。
