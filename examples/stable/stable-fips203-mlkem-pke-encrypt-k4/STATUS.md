# STATUS — stable-fips203-mlkem-pke-encrypt-k4

**语义**：FIPS 203 **Alg.14 全链 K-PKE.Encrypt**（k=4，ML-KEM-1024 PKE）— **stable 定型交付算子**（自 `exp-fips203-mlkem-pke-encrypt-k4` 复制晋级，2026-07-09）。

**实现方案**：[`stable-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.pdf`](stable-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.pdf)  
**baseline-registry**：[`docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md)  
**验收权重**：[交付口径：CPU 辅助 / SIM 主参考](../../../docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)

## 唯一交付路径

| 项 | 内容 |
|----|------|
| **入口** | `bash run.sh` → `ascendc_kernels_bbit` |
| **Launch** | **SIM 2**（prep → l18_l19 含内联 pack）= **生产主路径**；CPU 5（v=`golden_v` 注入）= **辅助孪生** |
| **I/O** | `input/{ek_pke,m,coins,lut_*}` → `output/c.bin` **仅**密文 1568B；中间态不落盘 |

## 验收

| 项 | 权重 | 结果 | 证据 |
|----|------|------|------|
| **SIM** | **主参考** | ✅ | Total tick **627590**（910B4）；`c max=0`；根目录无 stray dump |
| CPU | 辅助 | ✅ | `run.sh -r cpu` `c max=0`（非与 SIM 同构；依赖 `golden_v`） |
| liboqs KAT | 批测 | ✅ | `kat_liboqs_vs_ascendc.sh` **CPU×10 + SIM×1** PASS |
| round-trip | 批测 | ✅ | `scripts/roundtrip_pke_batch.sh` **CPU×10 + SIM×1** PASS |

默认 **无需** 手动 `export SIM_DIRECT` / `HAT_*`。无 NPU 实机前，**交付结论以 SIM 为准**。

```bash
cd examples/stable/stable-fips203-mlkem-pke-encrypt-k4
bash run.sh -r sim -v Ascend910B4          # 主参考
bash run.sh -r cpu -v Ascend910B4          # 辅助
bash kat_liboqs_vs_ascendc.sh
bash scripts/roundtrip_pke_batch.sh        # 仓库根
```

**预研副本**：[`exp-fips203-mlkem-pke-encrypt-k4`](../../incubating/exp-fips203-mlkem-pke-encrypt-k4/)（保留）  
**探针对照**：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../../../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/)
