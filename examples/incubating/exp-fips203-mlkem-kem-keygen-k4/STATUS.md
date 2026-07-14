# STATUS — exp-fips203-mlkem-kem-keygen-k4

FIPS 203 **Algorithm 19 `ML-KEM.KeyGen()`**（ml_kem_1024 / k=4）— incubating 预研副本。

| 项 | 值 |
|----|-----|
| **状态** | **已晋级** [`stable-fips203-mlkem-kem-keygen-k4`](../../stable/stable-fips203-mlkem-kem-keygen-k4/)（2026-07-14 `#交付#`）；**交付以 stable 为准** |
| **customspec** | [`exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf`](exp-fips203-mlkem-kem-keygen-k4-实现方案-customspec.pdf)（预研稿保留） |
| **Launch** | **2**（prep ‖ mmad+KemKgTailFused） |
| **I/O** | `seed_d`+LUT → `ek_kem` 1568B / `dk_kem` 3168B |
| **SIM tick** | **707057**（晋级前 incubating；复测 706657） |

## 晋级前验收摘要（2026-07-14）

| 门禁 | 结果 |
|------|------|
| CPU 压测 ×40 | PASS |
| SIM_DIRECT | PASS；tick **707057** |
| vs correctness ×10 | PASS |
| liboqs CPU×10 + SIM×3（家里） | PASS |
| liboqs CPU×10 + SIM×1（复测） | PASS |

本目录副本**保留**供对照；新改动请落在 **stable**（或开新 `stable-…-vN`）。
