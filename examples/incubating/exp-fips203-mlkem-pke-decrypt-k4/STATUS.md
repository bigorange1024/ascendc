# STATUS — exp-fips203-mlkem-pke-decrypt-k4

**阶段**：【预研】**完成** → 已 **`#交付#` 复制晋级** [`stable-fips203-mlkem-pke-decrypt-k4`](../../stable/stable-fips203-mlkem-pke-decrypt-k4/)（2026-07-10）  
**customspec**：[`exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex`](exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex)（+ PDF）  
**交付以 stable 为准**；本目录为预研副本。

FIPS 203 **Alg.15 K-PKE.Decrypt**（ml_kem_1024 / k=4）**预研用例**。

| 项 | 值 |
|----|-----|
| **基线** | [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../../ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/) |
| **I/O** | **生产** `input/`：`dk_pke`+`c`+`lut_*` → **仅** `output/m.bin` |
| **SEED_D** | 20260619 |
| **Launch** | **1×** `f203_decrypt_device_fused`（MIX `aicore=1`） |
| **SIM tick** | **283290** |

## 验收证据（2026-07-10）

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | `m max=0` |
| SIM | `bash run.sh -r sim -v Ascend910B4` | `m max=0`；tick **283290** |
| liboqs KAT | `bash kat_liboqs_vs_ascendc.sh` | **CPU×10 + SIM×1 PASS** |
| round-trip | `bash roundtrip_pke_batch.sh` | **CPU×10 + SIM×1 PASS** |

```bash
cd examples/incubating/exp-fips203-mlkem-pke-decrypt-k4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
```

**定型交付**：[`stable-fips203-mlkem-pke-decrypt-k4`](../../stable/stable-fips203-mlkem-pke-decrypt-k4/)
