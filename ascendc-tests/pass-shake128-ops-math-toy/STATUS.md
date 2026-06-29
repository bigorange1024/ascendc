# STATUS — pass-shake128-ops-math-toy

| 项 | 值 |
|----|-----|
| **目的** | CPU/SIM/NPU 向量 **SHAKE128**（shim 轨）；与 SHAKE256 探针共用 `shared/` 核 |
| **设备代码** | `library/shared/shake_xof_kernel` + `library/shared/keccak_f1600_kernel` |
| **CPU 孪生** | **PASS** |
| **SIM (CAModel)** | **PASS**（abc **12285** tick；batch_mixed PASS，2026-06-24 全 UB 自检） |

## 要点

- 薄入口 `shake128_toy_entry.cpp`；**参考实现** `shake128_toy_ub.hpp`（全 UB + `shake_ub_helpers.hpp`）
- `gen_data.py` → `auto_gen/toy_active_case.h`（随 `SHAKE128_CASE` 重建）
- Host golden：`tiny_sha3` + Python `shake_128`  
- tick 较 GM 直写基线（~10446）升高：GM 桥接 + 小 buffer 标量搬运；算法路径未变
- **下游**：设备批量 PRF 前置探针 → [`fix-f203-alg13-host-scalar-fullchain-k4/DEVICE_PRF_BATCH_PLAN.md`](../pass-fix-f203-alg13-lines8-15-se-k4/INTEGRATION.md)

## 验证

```bash
cd ascendc-tests/pass-shake128-ops-math-toy
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
SHAKE128_CASE=prf_sigma_n0 bash run.sh -r sim -v Ascend910B4
```
