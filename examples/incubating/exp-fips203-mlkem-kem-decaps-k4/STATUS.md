# STATUS — exp-fips203-mlkem-kem-decaps-k4

FIPS 203 **Alg.21 / Alg.18 Decaps**（ml_kem_1024 / k=4）— incubating 自包含预研副本。

| 项 | 值 |
|----|-----|
| **customspec** | [`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex`](exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex) |
| **阶段** | **已晋级** [`stable-fips203-mlkem-kem-decaps-k4`](../../stable/stable-fips203-mlkem-kem-decaps-k4/)（2026-07-20 `#交付#`）；本目录保留 |
| **Decrypt** | 本目录 `decrypt/` **vendored** |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored** |
| **KEM** | `kem/`：`G` 并入 Phase-E prep；设备 FO（SIM 过渡 `fo_only`） |
| **Launch** | SIM 4 / CPU 6 |
| **I/O** | `dk_kem`+`c`+LUT → **仅** `K` |
| **SIM tick** | **1032789**（D **286999** + E **745790**） |

## 验收证据（晋级前，2026-07-20）

| 模式 | 结果 |
|------|------|
| CPU / SIM | **PASS** |
| device roundtrip | **PASS** CPU+SIM（含拒绝） |
| liboqs 分项 KAT | **PASS** CPU×10 + SIM×3 |

**交付以 stable 为准**；仓库 `DECAPS_DIR` 默认已切 stable。registry：[`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md)。
