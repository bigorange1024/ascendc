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

命令（默认全量，无需手写 env）：

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

对照：[INTEGRATION_PLAN.md](INTEGRATION_PLAN.md) · oracle [`fix-f203-alg20-kem-encaps-correctness-k4`](../fix-f203-alg20-kem-encaps-correctness-k4/)

## 后续（非本轮）

- 可选：更名 `pass-fix-f203-alg20-kem-encaps-device-k4`（对齐 KeyGen device 惯例）
- 仓库脚本 `ENCAPS_DIR` / kat 批测改指本探针
- T19b/c Decaps device
