# STATUS — pass-shake256-ascendc-toy

| 项 | 值 |
|----|-----|
| **目的** | 本地 AscendC **SHAKE256**（FIPS PRF 规范轨）；与 SHAKE128 探针共用 `shared/` |
| **设备代码** | `library/shared/shake_xof_kernel` + `library/shared/keccak_f1600_kernel` |
| **CPU 孪生** | **PASS**（abc、prf_sigma_n0） |
| **SIM** | **PASS**（abc；**12285** tick，2026-06-24 全 UB 自检） |

## 共享核

```
library/shared/shake_xof_kernel/
  shake_general.h              # ProcessOne；Init 接收 LocalTensor x/lengths/y
  shake_general_tiling_data.h
  tiling_host.hpp              # FillShakeTiling(..., rate)
pass-shake256-ascendc-toy/
  shake256_toy_ub.hpp          # toy 参考：UB + shake_ub_helpers（无 GM 载荷）
```

SHAKE128 探针同用 `shared/`，仅 `rate=168`（shim 轨）不同。

## 下游

- 原语已验收；**阶段一**集成探针 → [DEVICE_PRF_BATCH_PLAN.md](../pass-fix-f203-alg13-lines8-15-se-k4/INTEGRATION.md)（`fix-f203-alg13-se-device-scalar-k4`）

## 验证

```bash
cd ascendc-tests/pass-shake256-ascendc-toy
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
SHAKE256_CASE=prf_sigma_n0 bash run.sh -r sim -v Ascend910B4
SHAKE256_CASE=rate_136 bash run.sh -r cpu -v Ascend910B4
```
