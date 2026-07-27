# STATUS — pass-fix-f203-alg20-kem-encaps-device-k2

FIPS 203 **Algorithm 20 `ML-KEM.Encaps()`**（**ML-KEM-512 / k=2**）— W3/D20 设备探针。

| 项 | 值 |
|----|-----|
| **阶段** | **CPU+SIM PASS**（2026-07-27；glue-c 代码已同步） |
| **Encrypt** | 编译期引用活跃 [`pass-fix-f203-alg14-pke-encrypt-device-k2`](../pass-fix-f203-alg14-pke-encrypt-device-k2/)（`r←η1=3` / `e←η2=2`） |
| **KEM 头** | `kem/` 并入 prep：`m` GM → `H(ek)`/`G(m‖H(ek))` → `K`+`coins` |
| **Launch** | SIM **2** / CPU **5** |
| **I/O** | `ek_kem` **800B** + `m` **32B** + LUT → `c` **768B** + `K` **32B** |
| **布局** | **S-1 / true k=2**，`Â[4]`、`re[5]`，不补零 |
| **SIM tick** | 旧登记 **394978**；**glue-c 后待重登**（对照 incubating E20 **427927**） |
| **参数卡** | [`fips203-mlkem512-parameter-card.md`](../../../../docs/specs/fips203-mlkem512-parameter-card.md) |

## 实现说明

1. Launch-1：`K‖r = G(m‖H(ek))`，随后 prep Â[4] 与 `re[5]`（混合 CBD）。
2. Launch-2：NTT(r) + Â·r + INTT batch4 + `du=10`/`dv=4` pack。
3. `scripts/gen_data.py` 默认自生成 k2 `ek_kem`；`EK_KEM_SRC=` 可覆盖。

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**：`c.bin`/`K.bin` max=0（glue-c 后仍绿） |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**（正确性）；tick 待重登 |

## 后续

- D21/D21ct **已绿**；incubating E20 + liboqs glue **已绿**。
- 可选：重跑本探针 SIM 登记 glue-c 后 Total tick。
