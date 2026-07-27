# STATUS — exp-fips203-mlkem-kem-encaps-k4

FIPS 203 **Alg.20 / Alg.17 Encaps**（ml_kem_1024 / k=4）— incubating 自包含预研。

| 项 | 值 |
|----|-----|
| **customspec** | [`exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex`](exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex) |
| **阶段** | **已晋级** [`stable-fips203-mlkem-kem-encaps-k4`](../../stable/stable-fips203-mlkem-kem-encaps-k4/)（2026-07-15 `#验收#`）；本目录保留为预研副本 |
| **Encrypt** | 本目录 `prep/`+`compute/` **vendored**（自 stable Encrypt） |
| **KEM 头** | `kem/`：`m` GM → `H`/`G` → `K`‖`r`（并入 prep；非 Host 预填 `r`） |
| **Launch** | SIM 2 / CPU 5 |
| **I/O** | `ek_kem`+`m`+LUT → `c`+`K` |
| **SIM tick** | 交付登记 **721119**（stable）；对标 device **721010**；历史曾用全 0 `m` 测得 ~721k（**已废止默认全 0**） |

## 验收证据（2026-07-15）

| 模式 | 结果 |
|------|------|
| CPU | **PASS**（`c`/`K` max=0；默认 `m=urandom`；可用 `M_HEX`/`M_FILE` 定点） |
| SIM | **PASS**（`c`/`K` max=0；根目录无 stray dump） |
| golden | liboqs `encaps_derand`（`scripts/liboqs_kem_ref`） |
| liboqs 分项 KAT | **PASS**（固定 stash `ek` + 随机 `m`；**CPU×10 + SIM×3**；`c`/`K` 逐字节 = liboqs） |
| 默认 `m` | **禁止全 0**（2026-07-27）：`M_FILE` → `M_HEX` → 显式 `M_DEFAULT_HEX`（若全 0 → exit 1）→ `os.urandom(32)` |

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
# 分项 KAT（对齐 pass-fix device；勿改仓库默认 ENCAPS_DIR）
bash scripts/kem_keypair_stash_bootstrap.sh   # 一次性
ENCAPS_DIR=$PWD/examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4 \
  bash scripts/liboqs_kem_encaps_batch.sh
```

## 未做 / 非本轮

- Decaps；NPU 真机
- （本目录已晋级 stable；勿再以「默认全 0 `m`」作验收口径）
