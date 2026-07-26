# STATUS — pass-fix-f203-alg20-kem-encaps-device-k3

FIPS 203 **Algorithm 20 `ML-KEM.Encaps()`**（**ML-KEM-768 / k=3**）— W3/D20 设备探针。

| 项 | 值 |
|----|-----|
| **阶段** | **CPU+SIM PASS**（2026-07-26） |
| **Encrypt** | 编译期引用活跃 [`pass-fix-f203-alg14-pke-encrypt-device-k3`](../pass-fix-f203-alg14-pke-encrypt-device-k3/) |
| **KEM 头** | `kem/` 并入 `f203_kem_enc_prep` 前段：`m` GM 输入 → `H(ek)`/`G(m‖H(ek))` → `K`+`coins` |
| **Launch** | SIM **2** / CPU **5**（D14 k3 分段路径） |
| **I/O** | `ek_kem` **1184B** + `m` **32B** + LUT → `c` **1088B** + `K` **32B** |
| **SIM tick** | **592129**（Cloud / `SIM_DIRECT=1` / Ascend910B4） |
| **参数卡** | [`fips203-mlkem768-parameter-card.md`](../../../../docs/specs/fips203-mlkem768-parameter-card.md) §3.3 |

## 实现说明

1. Launch-1 在 block0 设备侧完成 `K‖r = G(m‖H(ek))`，随后复用 D14 prep 生成 Â[9] 与 `re[7]`。
2. Launch-2 复用 D14 k3 `f203_encrypt_l18_l19`：NTT(r) + Â·r + INTT batch4 + `du=10`/`dv=4` pack。
3. `scripts/gen_data.py` 默认自生成 k3 `ek_kem`；也可用 `EK_KEM_SRC=` 指定 1184B 公钥。`coins.bin` 仅供 CPU golden 注入，生产路径不读取 host coins。

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**：`c.bin`/`K.bin` max=0 |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**：Total tick **592129**；根无 stray dump |

## D21 阻塞 / 下一步

- D20 未发现锁定参数阻塞；D21 可按 §3.3 继续接 D15 k3 Decrypt + FO。
- 后续 D21ct 仍须保持 reject/CT 口径，不在本探针内展开。
