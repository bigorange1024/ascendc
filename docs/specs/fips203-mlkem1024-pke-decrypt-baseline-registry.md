# FIPS 203 ML-KEM-1024 PKE Decrypt — baseline-registry

**主题**：Alg.15 K-PKE.Decrypt（k=4）交付侧 golden / KAT 计算块登记  
**适用**：交付以 **`examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4`** 为准（2026-07-10 自 incubating 复制晋级）；预研副本 `examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4` 保留。  
**补登记**：2026-07-20（历史晋级时缺表；按现网 `scripts/gen_data.py` 生产契约回溯）。  
**规则**：生成 `input/` / `golden_*` 的计算内核**仅**可调用下表已验证来源；缺项须停下补登记。

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `dk_pke.bin` | 1536 B | `ByteEncode₁₂(ŝ)` |
| 输入 | `c.bin` | 1568 B | 密文 `c₁‖c₂` |
| 输入 | `lut_*_stacked.bin` | 静态 | NTT/INTT limb LUT（与 seed 无关） |
| 输出 / golden | `m.bin` / `golden_m.bin` | 32 B | **仅**明文；VERIFY 对拍用 `golden_m` |

夹具 `ek_pke` / `m` / `coins` 仅允许写在 `output/_gen_fixture/`（派生 `c`），**禁止**作为生产 `input/`。

---

## 2. 已验证计算块

| 计算块 | 用途 | 已验证来源 | 备注 |
|--------|------|------------|------|
| `gen_dk_pke(SEED_D)` | 造 `dk_pke` | 本目录 `scripts/host_golden/gen_dk_pke.py` | 与 KeyGen golden 同语义族 |
| `gen_ek_pke` + `expand_bytes` → m/coins | 夹具 | `gen_ek_pke.py` + `fips203_host_rng` | 仅 fixture |
| `golden_encrypt` → `c` | 合法密文 | `scripts/host_golden/golden_c.py` | **禁止** liboqs 作默认生产 golden |
| `golden_m`（明文期望） | VERIFY | 夹具 `m` 或同式 oracle | Host 仅 VERIFY |
| 静态 NTT/INTT LUT | `gen_data` | `load_lut_t_i8` | 与 Encrypt 同 LUT 族 |
| liboqs indcpa_dec / fixture | **KAT 对照** | `kat_liboqs_vs_ascendc.sh` + liboqs PKE ref | **仅** KAT |
| Decrypt 主体（设备） | Alg.15 | AscendC fused；I/O 对拍即可 | **禁止**参考 C 同构验收 |

---

## 3. 明确禁止

- 把 `ek`/`m`/`coins` 写进生产 `input/`。
- 在 golden 路径重写 NTT / Decompress / ByteDecode 核心。
- 把 liboqs 源码移植进 AscendC 并以同构验收。

---

## 4. 验收脚本（交付门禁）

| 脚本 | 默认 | 对拍对象 |
|------|------|----------|
| `bash run.sh -r cpu\|sim -v Ascend910B4` | 全量 | `m` vs `golden_m` |
| `bash kat_liboqs_vs_ascendc.sh` | CPU×10 + SIM×1 | AscendC `m` vs liboqs |
| `bash roundtrip_pke_batch.sh` | CPU×10 + SIM×1 | Encrypt→Decrypt 明文闭环 |

---

## 维护

变更 golden 计算来源或新增计算块 → 同步本表与 [`docs/specs/INDEX.md`](INDEX.md)。
