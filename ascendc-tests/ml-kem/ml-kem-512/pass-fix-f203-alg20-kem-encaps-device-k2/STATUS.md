# STATUS — pass-fix-f203-alg20-kem-encaps-device-k2

FIPS 203 **Algorithm 20 `ML-KEM.Encaps()`**（**ML-KEM-512 / k=2**）— W3/D20 设备探针。

| 项 | 值 |
|----|-----|
| **阶段** | **CPU+SIM PASS**（2026-07-27） |
| **Encrypt** | 编译期引用活跃 [`pass-fix-f203-alg14-pke-encrypt-device-k2`](../pass-fix-f203-alg14-pke-encrypt-device-k2/) |
| **KEM 头** | `kem/` 并入 `f203_kem_enc_prep` 前段：`m` GM 输入 → `H(ek)`/`G(m‖H(ek))` → `K`+`coins` |
| **Launch** | SIM **2** / CPU **5**（D14 k2 分段路径） |
| **I/O** | `ek_kem` **800B** + `m` **32B** + LUT → `c` **768B** + `K` **32B** |
| **布局** | **S-1 / true k=2**，沿用 D14 k2 `Â[4]`、`re[5]`，不补零 |
| **SIM tick** | **394978**（Cloud / `SIM_DIRECT=1` / Ascend910B4） |
| **参数卡** | [`fips203-mlkem512-parameter-card.md`](../../../../docs/specs/fips203-mlkem512-parameter-card.md) §3.3 |

## 实现说明

1. Launch-1 在 block0 设备侧完成 `K‖r = G(m‖H(ek))`，随后复用 D14 prep 生成 Â[4] 与 `re[5]`。
2. Launch-2 复用 D14 k2 `f203_encrypt_l18_l19`：NTT(r) + Â·r + INTT batch4 + `du=10`/`dv=4` pack。
3. `scripts/gen_data.py` 默认自生成 k2 `ek_kem`；也可用 `EK_KEM_SRC=` 指定 800B 公钥。`coins.bin` 仅供 CPU golden 注入，生产路径不读取 host coins。

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**：`c.bin`/`K.bin` max=0 |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**：Total tick **394978**；根无 stray dump |

## D21 阻塞 / 下一步

- D20 未发现锁定参数阻塞；D21 可按 §3.3 继续接 D15 k2 Decrypt + FO。
- 后续 D21ct 仍须保持 reject/CT 口径，不在本探针内展开。
