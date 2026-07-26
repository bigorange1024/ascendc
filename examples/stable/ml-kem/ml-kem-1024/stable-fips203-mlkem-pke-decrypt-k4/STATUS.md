# STATUS — stable-fips203-mlkem-pke-decrypt-k4

**语义**：FIPS 203 **Alg.15 全链 K-PKE.Decrypt**（k=4，ML-KEM-1024 PKE）— **stable 定型交付算子**（自 `exp-fips203-mlkem-pke-decrypt-k4` 复制晋级，2026-07-10）。

**实现方案**：[`stable-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.pdf`](stable-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.pdf)

**baseline-registry**：[`docs/specs/fips203-mlkem1024-pke-decrypt-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-pke-decrypt-baseline-registry.md)（2026-07-20 补登记）

## 唯一交付路径

| 项 | 内容 |
|----|------|
| **入口** | `bash run.sh` → `ascendc_kernels_bbit` |
| **Launch** | **1×** `f203_decrypt_device_fused`（MIX `aicore=1`；GATE 4/8） |
| **I/O** | `input/{dk_pke,c,lut_*}` → `output/m.bin` **仅**明文 32B；造 c 夹具在 `output/_gen_fixture/`（不进 input） |

## 验收

| 项 | 结果 | 证据 |
|----|------|------|
| **SIM** | ✅ | Total tick **283290**（910B4）；`m max=0`；根目录无 stray dump |
| CPU | ✅ | `run.sh -r cpu` `m max=0` |
| liboqs KAT | ✅ | `kat_liboqs_vs_ascendc.sh` **CPU×10 + SIM×1** PASS（liboqs keygen + host golden_c fixture） |
| round-trip | ✅ | `bash roundtrip_pke_batch.sh` **CPU×10 + SIM×1** PASS |

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4
bash run.sh -r sim -v Ascend910B4
bash run.sh -r cpu -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
bash roundtrip_pke_batch.sh
```

**预研副本**：[`exp-fips203-mlkem-pke-decrypt-k4`](../../incubating/exp-fips203-mlkem-pke-decrypt-k4/)（保留）  
**探针对照**：[`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/)
