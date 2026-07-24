# STATUS — stable-fips203-mlkem-kem-decaps-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — **定型交付 v1**（2026-07-24 `#交付#` 自 incubating 整树复制）。

| 项 | 值 |
|---|---|
| **来源** | [`exp-fips203-mlkem-kem-decaps-k4`](../../incubating/exp-fips203-mlkem-kem-decaps-k4/) |
| **customspec** | [`stable-…-实现方案-customspec.tex`](stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex) |
| **registry** | [`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md) |
| **行为基线** | [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)（仍作行为基线；交付以本目录为准） |
| **PKE** | 本目录 `pke_decrypt/` + `prep/`/`compute/` **vendored** |
| **SIM host** | 生产默认 **`ASCENDC_SIM_HOST_MODE=decaps_2session`** |

## 验收（2026-07-24 Cloud 本目录复验）

| 范围 | 命令 | 结果 |
|------|------|------|
| **全链 CPU** | `bash run.sh -r cpu -v Ascend910B4` | `K` **max=0** **PASS** |
| **全链 SIM** | `SIM_DIRECT=1 bash run.sh -r sim …` | `K` **max=0** **PASS**；D tick **286866** + E **763780**；根无 stray dump |
| **拒绝 CPU** | `KEM_DECAPS_REJECT=1` | `REJECT PASS`（`K==J(z‖c)`） |
| **拒绝 SIM** | `KEM_DECAPS_REJECT=1 SIM_DIRECT=1 …` | `REJECT PASS`；D≈**286679** + E≈**763735**；根无 stray |
| **liboqs KAT** | `bash scripts/liboqs_kem_decaps_batch.sh` | **CPU×10 + SIM×3 PASS**（须传 `M_FILE`；勿与其它 SIM 并行） |
| **roundtrip** | `bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu\|sim` | **CPU+SIM PASS**（合法 agreement + Gate E3 拒绝） |

```bash
cd examples/stable/stable-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 拒绝 / KAT / roundtrip 见仓库 scripts/ 与 qa 纪要
```
