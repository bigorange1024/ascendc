# FIPS 203 ML-KEM-1024 KEM Decaps — baseline-registry

**主题**：Alg.21 `ML-KEM.Decaps()`（经 Alg.18；k=4）交付侧 golden / KAT 计算块登记  
**适用**：交付以 **`examples/stable/stable-fips203-mlkem-kem-decaps-ct-k4`** 为准（2026-07-24 `#交付#` 自 incubating 复制晋级）；预研副本 `examples/incubating/exp-fips203-mlkem-kem-decaps-ct-k4` 保留。  
**规则**：生成 `input/` / `golden_*` 的计算内核**仅**可调用下表已验证来源；缺项须停下补登记。

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `dk_kem.bin` | 3168 B | `dk_pke‖ek‖h‖z`；可由 KeyGen / liboqs `keypair_derand` / stash 引导 |
| 输入 | `c.bin` | 1568 B | 密文；合法路径由 liboqs `encaps_derand` 或 Encaps 产出 |
| 输入 | `lut_*_stacked.bin` | 静态 | NTT/INTT limb LUT（与消息无关；Decrypt+Encrypt 共用类） |
| 输出 / golden | `K.bin` | 32 B | 共享秘密 |

`$m'$` / `$h$` / `$z$` / `$K'$` / `$r'$` / `$c'$` **禁止**作为交付生产 I/O；Phase-E-only 灌 `m'` 仅调试（非默认）。

---

## 2. 已验证计算块

| 计算块 | 用途 | 已验证来源 | 备注 |
|--------|------|------------|------|
| `keypair_derand` / stash 造 `dk` | 缺 `DK` 时造钥 | `scripts/liboqs_kem_ref`（`keygen`） | 与 KeyGen/Encaps registry 同式 |
| `encaps_derand(ek,m)→c/K` | 合法路径造 `c` + oracle `K` | `scripts/liboqs_kem_ref`（`encaps`） | **仅** oracle / 向量生成 |
| `decaps(dk,c)→K` | golden `K`；拒绝路径对照 | 同 `liboqs_kem_ref`（`decaps`） | liboqs 0.15.0；**仅** oracle |
| Host `J(z‖c)` / `G` 对照 | VERIFY / 拒绝诊断 | Python `hashlib` / 与设备同式 SHA3/SHAKE | **禁止** Host 写生产 `K` |
| 静态 NTT/INTT LUT | `gen_data` | Encrypt/Decrypt 已登记 `host_golden` / `load_lut_t_i8` | 见 Encrypt / Decrypt registry |
| Decrypt 主体（设备） | Alg.18 行 5 | AscendC vendored；I/O 对拍即可 | **禁止**把 Decrypt C/Python 当 AscendC 规格 |
| Encrypt 主体（设备） | Alg.18 行 7 | AscendC vendored；I/O 对拍即可 | 同 Encaps / Encrypt registry |
| 设备 FO（比对 + `J` + 选 `K`） | Alg.18 行 8–12 | AscendC `kem/`；对拍 liboqs | **禁止** Host memcmp 冒充 |

Decrypt / Encrypt 内部计算块（NTT、CBD、Compress、ByteEncode 等）**沿用**已登记的 PKE Encrypt / Decrypt / Encaps registry；本表不重复展开，**禁止**未登记重写。

---

## 3. 明确禁止

- 在 golden 路径重写 NTT / CBD / ByteEncode / Compress / FO 核心。
- Host 预填生产 `$r'$` / 伪 `$G$`/`$J$` 作默认路径。
- 把 liboqs / correctness / frozen 源码逐步移植进 AscendC 并写「与 xxx 一致」验收。
- 使用未在 Encrypt/Decrypt/本表登记的 LUT 或哈希实现作为 golden 内核。

---

## 4. 验收脚本（交付门禁）

| 脚本 | 默认 | 对拍对象 |
|------|------|----------|
| `bash run.sh -r cpu\|sim -v Ascend910B4` | 全量 + VERIFY；SIM `decaps_2session` | `K` vs `golden/` / liboqs |
| `KEM_DECAPS_REJECT=1 bash run.sh -r cpu …` | 调试/拒绝门 | `K` vs liboqs ≡ `J(z‖c)` |
| `bash scripts/liboqs_kem_decaps_batch.sh` | 默认 `DECAPS_DIR`→stable | 固定 stash `dk` + 随机 `c` ↔ liboqs |

---

## 维护

变更 golden 计算来源或新增计算块 → 同步本表与 `docs/specs/INDEX.md`。
