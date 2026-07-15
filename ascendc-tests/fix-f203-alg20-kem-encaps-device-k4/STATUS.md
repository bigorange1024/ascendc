# STATUS — fix-f203-alg20-kem-encaps-device-k4

FIPS 203 **Alg.20 / Alg.17 Encaps**（ml_kem_1024 / k=4）— **无 vendor** 设备主线（T19a）。

| 项 | 值 |
|----|-----|
| **阶段** | **PASS**（2026-07-15）：CPU + SIM `c`/`K` max=0 |
| **Encrypt** | 编译期引用 [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) |
| **KEM 头** | `kem/` 并入 `f203_kem_enc_prep` 前段：`m` GM 输入 → `H`/`G` → `K`+`coins` |
| **Launch** | SIM 2 / CPU 5（= stable Encrypt） |
| **I/O** | `ek_kem`+`m`+LUT → `c`+`K` |
| **SIM tick** | **721010**（`SIM_DIRECT=1`，2026-07-15） |

## 验收

| 模式 | 状态 |
|------|------|
| CPU | **PASS**（`c`/`K` max=0） |
| SIM | **PASS**（`c`/`K` max=0；Total tick **721010**；根目录无 stray dump） |
| liboqs 分项 KAT | **PASS**（2026-07-15：`CPU×10 + SIM×3`；固定 stash `ek` + 随机 `m` ↔ `encaps_derand`） |

命令（默认全量，无需手写 env）：

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
# 分项 KAT（默认 ENCAPS_DIR=本探针；默认 CPU×10+SIM×3）
bash scripts/kem_keypair_stash_bootstrap.sh   # 一次性
bash scripts/liboqs_kem_encaps_batch.sh
```

对照：[INTEGRATION_PLAN.md](INTEGRATION_PLAN.md) · oracle [`fix-f203-alg20-kem-encaps-correctness-k4`](../fix-f203-alg20-kem-encaps-correctness-k4/)

## 后续（非本轮）

- 可选：更名 `pass-fix-f203-alg20-kem-encaps-device-k4`
- ~~仓库脚本 `ENCAPS_DIR` / kat 批测改指本探针~~ → **已改**（`liboqs_kem_encaps_batch.sh` / `kat_liboqs_kem_encaps.py` 默认本目录；`CPU×10+SIM×3` PASS）
- T19b/c Decaps device
