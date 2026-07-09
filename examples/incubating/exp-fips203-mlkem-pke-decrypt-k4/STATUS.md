# STATUS — exp-fips203-mlkem-pke-decrypt-k4

FIPS 203 **Alg.15 K-PKE.Decrypt**（ml_kem_1024 / k=4）**预研用例**。

| 项 | 值 |
|----|-----|
| **customspec** | [`exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex`](exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex) |
| **基线** | [`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../../ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/)（一次性 vendor） |
| **阶段** | **【预研】CPU+SIM PASS**（2026-07-09） |
| **I/O** | **生产** `input/`：`dk_pke`+`c`+`lut_*` → **仅** `output/m.bin`；造 c 夹具在 `output/_gen_fixture/`（不进 input） |
| **SEED_D** | 20260619 |
| **Launch** | **1×** `f203_decrypt_device_fused`（MIX `aicore=1`；GATE 4/8） |
| **SIM tick** | **283290** |
| **注释** | 主路径（main / fused / unpack / decode / su_dot / tail / gen_data）已补详细中文注释（2026-07-09） |

## 验收

| 模式 | 结果 |
|------|------|
| CPU | `m` max=0 |
| SIM | `m` max=0；tick **283290**；根目录无 stray dump |

```bash
cd examples/incubating/exp-fips203-mlkem-pke-decrypt-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## 下一步（家里 Agent）

1. **KAT**：参照 Encrypt `kat_liboqs_vs_ascendc.sh`，补 Decrypt↔liboqs 门禁
2. **round-trip**：`DECRYPT_DIR` 指本目录跑 `scripts/roundtrip_pke_batch.sh`
3. **`#交付#`** → `examples/stable/stable-fips203-mlkem-pke-decrypt-k4`（T15a）
