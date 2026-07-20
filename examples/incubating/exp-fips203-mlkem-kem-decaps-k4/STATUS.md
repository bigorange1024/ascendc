# STATUS — exp-fips203-mlkem-kem-decaps-k4

FIPS 203 **Alg.21 / Alg.18 Decaps**（ml_kem_1024 / k=4）— incubating 自包含预研。

| 项 | 值 |
|----|-----|
| **customspec** | [`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex`](exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex) |
| **阶段** | **【预研】CPU ✓ / SIM ✓**（2026-07-20） |
| **Decrypt** | 本目录 `decrypt/` **vendored** |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored** |
| **KEM** | `kem/`：`G` 并入 Phase-E prep；设备 FO（SIM 过渡 `fo_only`） |
| **Launch** | SIM 4 / CPU 6 |
| **I/O** | `dk_kem`+`c`+LUT → **仅** `K` |
| **SIM tick** | D **286999** + E **745790**（全链合法；对标 pass-fix D**286803**+E**745925**） |

## 验收证据（2026-07-20）

| 模式 | 结果 |
|------|------|
| CPU | **PASS**（`K` max=0） |
| SIM | **PASS**（`K` max=0；单库+`decaps_1session`；根目录无 stray dump） |
| golden | liboqs encaps → Decaps 对拍 |

```bash
cd examples/incubating/exp-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## 未做 / 非本轮

- `#交付#` → `examples/stable/stable-…-kem-decaps-k4`
- SIM `fo_only` 收回 `l18_l19` 尾（4→3 launch）
- liboqs 分项 KAT CPU×10+SIM×3（可复用仓库 `scripts/liboqs_kem_decaps_batch.sh` + `DECAPS_DIR=`）
- NPU 真机
