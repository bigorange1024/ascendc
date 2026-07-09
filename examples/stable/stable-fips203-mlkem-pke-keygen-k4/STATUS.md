# STATUS — stable-fips203-mlkem-pke-keygen-k4

**语义**：FIPS 203 **Alg.13 全链 KeyGen**（k=4，ML-KEM-768 PKE）— **stable 定型交付算子**（自 `exp-fips203-mlkem-pke-keygen-k4` 复制晋级，2026-06-29）。

**实现方案**：[`stable-fips203-mlkem-pke-keygen-k4-实现方案-customspec.pdf`](stable-fips203-mlkem-pke-keygen-k4-实现方案-customspec.pdf)

## 唯一交付路径

| 项 | 内容 |
|----|------|
| **入口** | `bash run.sh` → `ascendc_keygen_bbit` |
| **Launch** | ① `f203_keygen_prep`（AIV×2，双 AIV 并行 Â）② `compute/mmad_custom`（MIX 1AIC+2AIV，`F203_KEYGEN_EK_PKE=1`） |
| **I/O** | `input/` seed+LUT → `output/` ek_pke + dk_pke |

## 验收

| 项 | 结果 | 证据 |
|----|------|------|
| CPU | ✅ | `run.sh -r cpu` ek/dk 尺寸 OK |
| SIM | ✅ | Total tick **542393**（910B4） |
| liboqs KAT | ✅ | CPU 10/10 · SIM 1/1 |


默认 **无需** 手动 `export SIM_DIRECT` / `HAT_*`（`run.sh -r sim` 内已设置全量生产路径）。

```bash
cd examples/stable/stable-fips203-mlkem-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
```

**典型 SIM**（Ascend910B4）：total_tick ≈ **542328**。

**探针对照**：[`pass-fix-f203-alg13-device-keygen-k4`](../../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/)

**技术总结**：[docs/notes/F203-KeyGen-exp交付示例技术总结.md](../../../docs/notes/F203-KeyGen-exp交付示例技术总结.md) · [docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md](../../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)
