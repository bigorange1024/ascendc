# STATUS — exp-fips203-mlkem-kem-encaps-k4

FIPS 203 **Alg.20 / Alg.17 Encaps**（ml_kem_1024 / k=4）— incubating 自包含预研。

| 项 | 值 |
|----|-----|
| **customspec** | [`exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex`](exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex) |
| **阶段** | **有条件完成 / CPU+SIM PASS**（2026-07-15【预研】） |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored**（自 stable Encrypt） |
| **KEM 头** | `kem/`：`m` GM → `H`/`G` → `K`‖`r`（并入 prep；非 Host 预填 `r`） |
| **Launch** | SIM 2 / CPU 5 |
| **I/O** | `ek_kem`+`m`+LUT → `c`+`K` |
| **SIM tick** | **721211**（全 0 `m`）；**721033**（定点非零 `m`）；复测 **721102**；对标 device **721010** |

## 验收证据（2026-07-15）

| 模式 | 结果 |
|------|------|
| CPU | **PASS**（`c`/`K` max=0；含默认 `m=0` + **随机 `m`×3**） |
| SIM | **PASS**（`c`/`K` max=0；根目录无 stray dump） |
| golden | liboqs `encaps_derand`（`scripts/liboqs_kem_ref`） |

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
M_RANDOM=1 bash run.sh -r cpu -v Ascend910B4   # 可选多轮
```

## 未做 / 非本轮

- `#交付#` → `examples/stable/stable-…-kem-encaps-k4`
- Decaps；NPU 真机
- 仓库级 `scripts/liboqs_kem_encaps_batch.sh` 默认改指本目录（仍指 pass-fix device）
