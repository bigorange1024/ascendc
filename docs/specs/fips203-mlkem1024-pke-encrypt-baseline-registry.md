# FIPS 203 ML-KEM-1024 PKE Encrypt — baseline-registry

**主题**：Alg.14 K-PKE.Encrypt（k=4）交付侧 golden / KAT 计算块登记  
**适用**：`examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4`、晋级后 `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4`  
**规则**：生成 `input/` / `golden/` 的计算内核**仅**可调用下表已验证来源；缺项须停下补登记（见 Rule「Golden、基准与 baseline-registry」）。

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `ek_pke.bin` | 1568 B | 公钥（ByteEncode₁₂(t̂)‖ρ） |
| 输入 | `m.bin` | 32 B | 明文 |
| 输入 | `coins.bin` | 32 B | Encrypt 随机性 |
| 输入 | `lut_*_stacked.bin` | 静态 | NTT/INTT limb LUT（与 seed 无关） |
| 输入 | `golden_v.bin` | 仅 CPU | **非** Alg.14 产物；CPU 分段 pack 注入 |
| 输出 / golden | `c.bin` | 1568 B | 密文唯一验收对象 |

中间态 Â/y/u/v 等**禁止**作为交付 I/O 落盘。

---

## 2. 已验证计算块

| 计算块 | 用途 | 已验证来源 | 备注 |
|--------|------|------------|------|
| `golden_encrypt(ek,m,coins)→c` | host golden / prepare 自检 | 本目录 `scripts/host_golden/golden_c.py` | 禁止 liboqs 作生产 golden |
| `decode_t_hat` / `build_re` / `pack_ciphertext` | Alg.14 子步骤 | 同上 + `f203_ref_common.py` | |
| Stage1–3 NTT/INTT（host） | golden_v、中间参考 | `f203_ref_common.stage123_transform` + `thirdparty/ntt_onnx/.../transpose_mlkem_luts_i8.h` | LUT 只读 |
| `gen_ek_pke(SEED_D)` | 缺 fixture 时本地造 ek | `scripts/host_golden/gen_ek_pke.py` | 与 KeyGen golden 同语义 |
| m/coins 派生 | SHAKE256 域分离扩字节 | `library/shared/fips203_host_rng/host_rng.py` `expand_bytes` | 定点 `SEED_D=` 可覆盖；默认 SHA3 派生 `SEED_D` |
| liboqs indcpa_enc | **KAT 对照 oracle** | `scripts/liboqs_pke_ref`（encrypt） | **仅** KAT；非 AscendC 实现规格 |
| liboqs fixture | ek/m/coins/c 向量 | `scripts/liboqs_pke_fixture.py` | KAT / 交叉验证 |

---

## 3. 明确禁止

- 在 golden 路径重写 NTT 核心、自行发明 Compress/ByteEncode 公式（须用登记表 API）。
- 把 liboqs / mlkem-native 源码逐步移植进 AscendC 并写「与 xxx 一致」验收。
- 用 `golden_v` 冒充 Alg.14 输出或 SIM 全设备依赖。

---

## 4. 验收脚本（交付门禁）

| 脚本 | 默认 | 对拍对象 |
|------|------|----------|
| `bash run.sh -r cpu\|sim -v Ascend910B4` | 全量 | `output/c` vs `golden/c` |
| `bash kat_liboqs_vs_ascendc.sh` | CPU×10 + SIM×1 | AscendC `c` vs liboqs `c` |
| `bash scripts/roundtrip_pke_batch.sh` | CPU×10 + SIM×1 | Decrypt(m) vs Encrypt 明文（Encrypt=本算子） |

**权重（无 NPU）**：**SIM 为主参考**；CPU 为辅助正确性（可依赖 `golden_v`，非与 SIM 同构）。详见 [`docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md`](../notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)。

---

## 维护

变更 golden 计算来源或新增计算块 → 同步本表与 `docs/specs/INDEX.md`。
