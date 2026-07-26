# STATUS — pass-fix-f203-alg19-kem-keygen-device-k3

FIPS 203 **Algorithm 19 `ML-KEM.KeyGen()`**（**ML-KEM-768 / k=3**）— W3/D19 设备探针。

| 项 | 值 |
|---|---|
| **阶段** | **CPU+SIM PASS**（2026-07-26） |
| **Launch** | **2**：prep AIV_ONLY `blockDim=2` → compute MIX `blockDim=1` + Alg.16 尾内嵌 |
| **PKE 源** | 本目录复制活跃 D13 k3 PKE KeyGen 源码；`scripts/compute`/`thirdparty` 软链到 D13 |
| **I/O** | `ek_kem` **1184B** · `dk_kem` **2400B**；`SEED_D=20260619` |
| **Derand** | `d`: `exp-mlkem-f203-2s1e-k3:SEED_D=`；`z`: `exp-mlkem-f203-kem-k3:SEED_Z=` |
| **SIM tick** | **510775**（Cloud / `SIM_DIRECT=1` / Ascend910B4） |
| **参数卡** | [`fips203-mlkem768-parameter-card.md`](../../../../docs/specs/fips203-mlkem768-parameter-card.md) §3.3 |

## 实现说明

1. Launch-1 复用 D13 prep：Â[9] + polyvec6（s‖e），双 AIV 5+4 分片。
2. Launch-2 复用 D13 compute：polyvec6 NTT → Inner 2+1 → ByteEncode12 3×384 → `ek_pke`。
3. D19 尾段在同一 MIX kernel 内由 AIV0 执行：`dk_kem = dk_pke(1152)‖ek(1184)‖H(ek)(32)‖z(32)`，无第三次 launch。

## 验收

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**：`ek_kem`/`dk_kem` max=0 |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**：Total tick **510775**；根无 stray dump |

## D20 阻塞

- D19 通过后，D20 可按同一 W3 参数卡继续接 D14 k3 Encrypt；当前无额外参数阻塞。
