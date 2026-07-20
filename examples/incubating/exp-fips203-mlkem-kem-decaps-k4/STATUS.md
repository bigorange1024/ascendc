# STATUS — exp-fips203-mlkem-kem-decaps-k4

FIPS 203 **Alg.21 / Alg.18 Decaps**（ml_kem_1024 / k=4）— incubating 自包含预研副本。

| 项 | 值 |
|----|-----|
| **customspec** | [`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex`](exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex)（**T19i** 修订：SIM 3） |
| **阶段** | **已晋级** [`stable-fips203-mlkem-kem-decaps-k4`](../../stable/stable-fips203-mlkem-kem-decaps-k4/)（2026-07-20 `#交付#`）；本目录保留并 **【迭代】T19i** |
| **Decrypt** | 本目录 `decrypt/` **vendored** |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored**；`l18_l19` 以 `kem/` 覆盖为准 |
| **KEM** | `kem/`：`G` 并入 Phase-E prep；SIM FO 并入 `l18_l19` 尾（T19i） |
| **Launch** | SIM **3** / CPU 6 |
| **I/O** | `dk_kem`+`c`+LUT → **仅** `K` |
| **SIM tick（T19i）** | **1050781**（D **286846** + E **763935**） |

## 验收证据

### 晋级前（2026-07-20，SIM 4）

| 模式 | 结果 |
|------|------|
| CPU / SIM | **PASS** |
| device roundtrip | **PASS** CPU+SIM（含拒绝） |
| liboqs 分项 KAT | **PASS** CPU×10 + SIM×3 |

### T19i【迭代】（2026-07-20，SIM 3）

| 门禁 | 结果 |
|------|------|
| `bash run.sh -r cpu` | **PASS**（`K` max=0） |
| `SIM_DIRECT=1 bash run.sh -r sim` | **PASS**；D**286846**+E**763935**；根无 stray |
| `KEM_DECAPS_REJECT=1` cpu | **PASS** |
| `KEM_DECAPS_REJECT=1` sim | **PASS** |

**交付默认仍以 stable 为准**（stable 尚为 SIM 4，待 `$规格$`/`#修改#` 镜像）。registry：[`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md)。
