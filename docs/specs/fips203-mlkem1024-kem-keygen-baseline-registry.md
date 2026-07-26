# FIPS 203 ML-KEM-1024 KEM KeyGen — baseline-registry

**主题**：Alg.19 `ML-KEM.KeyGen()`（经 Alg.16 → Alg.13；k=4）交付侧 golden / KAT 计算块登记  
**适用**：交付以 **`examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4`** 为准（2026-07-14 `#交付#` 自 incubating 复制晋级）；预研副本 `examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4` 保留。  
**规则**：生成 `input/` / `golden_*` 的计算内核**仅**可调用下表已验证来源；缺项须停下补登记。

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `seed_d.bin` | 4 B | `uint32` LE；**默认** host SHA3（`library/shared/fips203_host_rng`）；`SEED_D=` 可覆盖定点（如旧 KAT `20260619`） |
| 输入 | `lut_even/odd_stacked.bin` | 静态 | NTT limb LUT（与 seed 无关） |
| 输出 / golden | `ek_kem.bin` | 1568 B | `= ek_PKE` |
| 输出 / golden | `dk_kem.bin` | 3168 B | liboqs 展开：`dk_pke‖ek‖H(ek)‖z` |

`d`/`z` **禁止**落盘；中间 Â/ŝ/ρ 等禁止作为交付 I/O。

---

## 2. 已验证计算块

| 计算块 | 用途 | 已验证来源 | 备注 |
|--------|------|------------|------|
| `DerandFromSeedD(seed_d)→d` | 与设备 d 同式 | `library/shared/fips203_se_sample/golden_se_sampling.py` | Host oracle 仅 VERIFY |
| `DerandZFromSeedD(seed_d)→z` | 与设备 z 同式 | `scripts/gen_data.py`：`SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z="‖十进制)` | 域分离串锁定 |
| `keypair_derand(d‖z)→ek/dk` | golden ek/dk | `scripts/liboqs_kem_ref`（`keygen`） | liboqs 0.15.0；**仅** oracle |
| 静态 NTT LUT | `prepare_production_input` | 本目录 `scripts/compute/gen_data.py`（`load_lut_t_i8`） | 与 PKE KeyGen 同 LUT |
| PKE KeyGen 主体（设备） | Alg.13 | AscendC vendored；I/O 对拍即可 | **禁止**把 PKE C/Python 当 AscendC 规格 |

---

## 3. 明确禁止

- 在 golden 路径重写 NTT / CBD / ByteEncode 核心。
- Host `tiny_sha3` / liboqs 参与**默认生产**路径写 `output/`（VERIFY/KAT 除外）。
- 把 liboqs 源码逐步移植进 AscendC 并写「与 xxx 一致」验收。

---

## 4. 验收脚本（交付门禁）

| 脚本 | 默认 | 对拍对象 |
|------|------|----------|
| `bash run.sh -r cpu\|sim -v Ascend910B4` | 全量 + VERIFY | `ek_kem`/`dk_kem` vs `golden_*` |
| 可选仓库 `scripts/liboqs_kem_vs_ascendc.sh` | 批量 | AscendC vs liboqs（旁路 A 等） |

---

## 维护

变更 golden 计算来源或新增计算块 → 同步本表与 `docs/specs/INDEX.md`。
