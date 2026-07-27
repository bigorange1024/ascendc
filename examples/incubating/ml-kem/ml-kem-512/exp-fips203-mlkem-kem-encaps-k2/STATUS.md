# STATUS — exp-fips203-mlkem-kem-encaps-k2

FIPS 203 **Algorithm 20 `ML-KEM.Encaps()`**（**ML-KEM-512 / k=2**）— W4b/E20 incubating。

| 项 | 值 |
|----|-----|
| **阶段** | **CPU+SIM PASS**（2026-07-27；glue-c 后） |
| **Encrypt** | 本目录 vendored D14 k2 Encrypt（`r←η1=3` / `e←η2=2`） |
| **KEM 头** | `kem/` 并入 prep：`m` GM → `H(ek)`/`G(m‖H(ek))` → `K`+`coins` |
| **Launch** | SIM **2** / CPU **5** |
| **I/O** | `ek_kem` **800B** + `m` **32B** + LUT → `c` **768B** + `K` **32B** |
| **布局** | **S-1 / true k=2**，`Â[4]`、`re[5]`，不补零 |
| **SIM tick** | **427927**（glue-c 后；Cloud / `SIM_DIRECT=1` / Ascend910B4） |
| **参数卡** | [`fips203-mlkem512-parameter-card.md`](../../../../docs/specs/fips203-mlkem512-parameter-card.md) |

## 实现说明

1. Launch-1 在 block0 设备侧完成 `K‖r = G(m‖H(ek))`，随后 prep 生成 Â[4] 与 `re[5]`（`r` 走 CBD3，`e₁‖e₂` 走 CBD2）。
2. Launch-2：NTT(r) + Â·r + INTT batch4 + `du=10`/`dv=4` pack。
3. `scripts/gen_data.py` 默认自生成 k2 `ek_kem`；也可用 `EK_KEM_SRC=` 指定 800B 公钥。

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**：`c.bin`/`K.bin` max=0 |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**：Total tick **427927**；根无 stray dump |
| liboqs | `USE_LIBOQS=1`（见 `scripts/exp_kem512_liboqs_roundtrip.sh`） | Encaps **c+K** max=0（CPU+SIM） |

## 后续

- 行为基线：[`pass-fix-f203-alg20-kem-encaps-device-k2`](../../../../ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg20-kem-encaps-device-k2/)（glue-c 后 tick **待重登**；旧登记 394978）。
- **不晋级** stable（须 `#交付#`）。
