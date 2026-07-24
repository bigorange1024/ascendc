# FIPS 203 ML-KEM-1024 KEM Decaps — baseline-registry

**主题**：Alg.21 `ML-KEM.Decaps()`（经 Alg.18 → Alg.15 Decrypt + Alg.14 Encrypt + FO；k=4）交付侧 golden / KAT 计算块登记  
**适用（交付默认）**：**`examples/stable/stable-fips203-mlkem-kem-decaps-k4`**（2026-07-20 `#交付#` 自 incubating 复制晋级）；预研副本 `examples/incubating/exp-fips203-mlkem-kem-decaps-k4` 保留。  
**行为基线（只读）**：`ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4`（勿当 golden 计算源 / 编译依赖）。  
**CT 专题副本（脚注）**：`research/formal-lang-dag` 上另有 **`-ct`** 三树（`pass-fix-…-device-ct-k4` / `exp-…-decaps-ct-k4` / `stable-…-decaps-ct-k4`），用于第7章 CT / 五指标实验；**非** `scripts/` 默认；golden 计算块登记与本文相同，勿混引用路径。  
**规则**：生成 `input/` / `golden/` 的计算内核**仅**可调用下表已验证来源；缺项须停下补登记。

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `dk_kem.bin` | 3168 B | `dk_PKE(1536)‖ek(1568)‖h(32)‖z(32)`；默认 `DK_KEM_SRC` / stash / KeyGen 产出 |
| 输入 | `c.bin` | 1568 B | 密文；合法路径由 liboqs encaps 或 `C_SRC` 提供 |
| 输入 | `lut_*_stacked.bin` | 静态 | Decrypt NTT/INTT + Encrypt NTT/INTT limb LUT（与消息无关） |
| 输出 / golden | `K.bin` | 32 B | 共享秘密（**唯一**生产输出） |

`$m'$` / `$K'$` / `$r'$`（coins）/ `$c'$` / `$h$` / `$z$` **禁止**作为交付 I/O 落盘；调试 dump（如 `m_prime.bin`）须标非默认。  
`input/coins.bin`、`input/golden_v.bin` **仅** CPU tikicpu 分段辅助，**非** SIM/生产契约。

---

## 2. 已验证计算块

| 计算块 | 用途 | 已验证来源 | 备注 |
|--------|------|------------|------|
| `encaps(ek,m)→c/K` | 合法路径造密文 + golden `K` | `scripts/liboqs_kem_ref`（`encaps`） | liboqs 0.15.0；**仅** oracle |
| `decaps(dk,c)→K` | 拒绝路径 / 交叉 golden | 同 `liboqs_kem_ref`（`decaps`） | 假密文下期望 ≡ `J(z‖c)` |
| `J(z‖c)=SHAKE256(z‖c, 32)` | Host 拒绝路径对照 | Python `hashlib.shake_256` | 与设备 FO `Shake256OneShot` 同式；**禁止** Host 写生产 `K` |
| `G(m'‖h)=SHA3-512` | Host 派生 coins / 调试对照 | Python `hashlib.sha3_512` | 仅 CPU `golden_v` / 对照；**禁止** Host 预填生产 `r'` |
| 静态 NTT/INTT LUT | `gen_data` | `scripts/host_golden` → `load_lut_t_i8` | 与 Encrypt/Decrypt 同 LUT 族 |
| CPU `golden_v` / `coins` | tikicpu Phase-E pack 辅助 | `host_golden/golden_c.py` + Stage123 + `G(m‖h)` | **仅** CPU；拒绝路径可用零填充占位 |
| Decrypt 主体（设备） | Alg.15 → `m'` | AscendC vendored `decrypt/`；I/O 对拍即可 | **禁止**把 Decrypt 参考 C 当 AscendC 规格 |
| Encrypt + FO（设备） | Alg.14 → `c'`；FO 选 `K` | AscendC vendored Encrypt + `kem/`；I/O 对拍即可 | SIM 过渡独立 `fo_only` 允许；**禁止** Host memcmp/SHA3 冒充 FO |

Decrypt / Encrypt 内部计算块（NTT、CBD、Compress、ByteEncode 等）**沿用**已登记的 PKE Encrypt / Decrypt / Encaps registry；本表不重复展开，**禁止**未登记重写。

---

## 3. 明确禁止

- 在 golden 路径重写 NTT / CBD / ByteEncode / Compress / Decompress / FO 核心。
- Host 预填生产 `r'`；Host 算 `G`/`J`/密文比对后写 `K.bin` 冒充设备 Decaps。
- 把 liboqs / frozen G4·G5 / correctness vendor 源码逐步移植进 AscendC，并以「与 xxx 一致」验收。
- 将 `m'` / `c'` / `K'` 作为交付物。

---

## 4. 验收脚本（交付门禁）

| 脚本 | 默认 / 建议 | 对拍对象 |
|------|-------------|----------|
| `bash run.sh -r cpu\|sim -v Ascend910B4` | 全量 + VERIFY；单库 + `decaps_1session` | `K` vs `golden/K.bin` |
| `KEM_DECAPS_REJECT=1 bash run.sh …` | 非默认；Gate E3 | `K` vs liboqs Decaps ≡ `J(z‖c)` |
| `bash scripts/liboqs_kem_decaps_batch.sh` | CPU×10 + SIM×3（`KEM_DEC_*_TRIALS`） | 固定 stash `dk` + liboqs 造 `c` ↔ device `K` |
| `bash scripts/roundtrip_kem_keygen_encaps_decaps.sh` | KeyGen device + Encaps stable + 本 Decaps | Encaps.`K` == Decaps.`K`；拒绝路径另验 |

仓库默认 `DECAPS_DIR` 指向 **`stable-fips203-mlkem-kem-decaps-k4`（无 `-ct`）**；可用 `DECAPS_DIR=` 覆盖回 incubating / [`pass-fix-…-decaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)。**`-ct`** 副本仅 CT 专题验收，不作 scripts 默认。

---

## 维护

变更 golden 计算来源、新增计算块、或晋级 stable → 同步本表与 [`docs/specs/INDEX.md`](INDEX.md)。
