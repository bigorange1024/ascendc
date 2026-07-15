# FIPS 203 ML-KEM-1024 KEM Encaps — baseline-registry

**主题**：Alg.20 `ML-KEM.Encaps()`（经 Alg.17 → Alg.14；k=4）交付侧 golden / KAT 计算块登记  
**适用**：交付以 **`examples/stable/stable-fips203-mlkem-kem-encaps-k4`** 为准（2026-07-15 `#验收#` 自 incubating 复制晋级）；预研副本 `examples/incubating/exp-fips203-mlkem-kem-encaps-k4` 保留。  
**规则**：生成 `input/` / `golden_*` 的计算内核**仅**可调用下表已验证来源；缺项须停下补登记。

---

## 1. 生产 I/O（黑盒）

| 角色 | 路径 | 尺寸 | 说明 |
|------|------|------|------|
| 输入 | `ek_kem.bin` | 1568 B | `= ek_PKE`；可由 `EK_KEM_SRC` / KeyGen 产出 / liboqs `keygen_derand` 引导 |
| 输入 | `m.bin` | 32 B | **Alg.17 消息**（GM 输入）；`M_FILE` / `M_HEX` / 默认定点可覆盖 |
| 输入 | `lut_*_stacked.bin` | 静态 | NTT/INTT limb LUT（与 `m` 无关） |
| 输出 / golden | `c.bin` | 1568 B | 密文 |
| 输出 / golden | `K.bin` | 32 B | 共享秘密 |

`$h$`/`$r$`/`$\hat{A}$`/`$y$`/`$e_{1}$`/`$e_{2}$` **禁止**作为交付 I/O；`golden/r_ref.bin` 仅 Host VERIFY / CPU `golden_v` 辅助，**非**设备生产输入。

---

## 2. 已验证计算块

| 计算块 | 用途 | 已验证来源 | 备注 |
|--------|------|------------|------|
| `H(ek)=SHA3-256` / `G(m‖h)=SHA3-512` | Host oracle（VERIFY） | Python `hashlib`（与设备 `Sha3OneShot` 同式） | 仅派生 `r_ref` / 对照；**禁止** Host 写生产 `r` |
| `encaps_derand(ek, m)→c/K` | golden `c`/`K` | `scripts/liboqs_kem_ref`（`encaps`） | liboqs 0.15.0；**仅** oracle |
| `keypair_derand` 引导 ek | 缺 `EK_KEM_SRC` 时造钥 | 同 `liboqs_kem_ref` `keygen` | 与 KeyGen registry 同式 `d‖z` |
| 静态 NTT/INTT LUT | `gen_data` | `scripts/host_golden` → `load_lut_t_i8` | 与 Encrypt 同 LUT |
| CPU `golden_v` | tikicpu 分段注入 v | `host_golden/golden_c.py` + Stage123 | **仅** CPU 辅助；非 SIM/生产契约 |
| Encrypt 主体（设备） | Alg.14 | AscendC vendored；I/O 对拍即可 | **禁止**把 Encrypt C/Python 当 AscendC 规格 |

---

## 3. 明确禁止

- 在 golden 路径重写 NTT / CBD / ByteEncode / Compress 核心。
- Host 预填生产 `r` / 伪 `H`/`G` 作默认路径。
- 把 liboqs 源码逐步移植进 AscendC 并写「与 xxx 一致」验收。

---

## 4. 验收脚本（交付门禁）

| 脚本 | 默认 | 对拍对象 |
|------|------|----------|
| `bash run.sh -r cpu\|sim -v Ascend910B4` | 全量 + VERIFY | `c`/`K` vs `golden/` |
| `bash scripts/liboqs_kem_encaps_batch.sh` | CPU×10 + SIM×3 | 固定 stash `ek` + 随机 `m` ↔ liboqs |

---

## 维护

变更 golden 计算来源或新增计算块 → 同步本表与 `docs/specs/INDEX.md`。
