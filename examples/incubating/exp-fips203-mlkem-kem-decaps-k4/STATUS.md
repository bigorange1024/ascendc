# STATUS — exp-fips203-mlkem-kem-decaps-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — incubating 自包含交付（`#交付#` 2026-07-24）。

| 项 | 值 |
|---|---|
| **阶段** | **CPU+SIM PASS**（合法路径 `K` max=0）；拒绝路径 CPU PASS |
| **customspec** | [`exp-…-实现方案-customspec.tex`](exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex) |
| **registry** | [`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md) |
| **行为基线** | [`pass-fix-f203-alg21-kem-decaps-device-k4`](../../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)（只读；禁止编译依赖） |
| **PKE** | 本目录 `pke_decrypt/` + `prep/`/`compute/` **vendored**（自 stable Decrypt/Encrypt） |
| **KEM** | `kem/`：`G(m'‖h)` 并入 Phase-E prep；设备 FO（SIM：`l18_l19` 同核；CPU：`pack_fo`） |
| **SIM host** | 生产默认 **`ASCENDC_SIM_HOST_MODE=decaps_2session`**；单库合库（`prepare_dec_shim` ← 本目录 `pke_decrypt`） |

## 验收（2026-07-24 Cloud）

| 范围 | 命令 | 结果 |
|------|------|------|
| **全链 CPU** | `bash run.sh -r cpu -v Ascend910B4` | `K` **max=0** **PASS** |
| **全链 SIM** | `SIM_DIRECT=1 bash run.sh -r sim …` | `K` **max=0** **PASS**；D tick **286829** + E **763658**；根无 stray dump |
| **拒绝 CPU** | `KEM_DECAPS_REJECT=1` | device `K` == liboqs == `J(z‖c)` **PASS** |

```bash
cd examples/incubating/exp-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 晋级

整树复制 → [`examples/stable/stable-fips203-mlkem-kem-decaps-k4/`](../../stable/stable-fips203-mlkem-kem-decaps-k4/)（v1）。
