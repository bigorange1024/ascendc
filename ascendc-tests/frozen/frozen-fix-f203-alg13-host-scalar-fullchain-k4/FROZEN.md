# ⛔ 已冻结（2026-06-26）

**原路径**：`ascendc-tests/fix-f203-alg13-host-scalar-fullchain-k4/`  
**继任**：
- 行 8–15 Host golden：[`library/shared/fips203_se_sample/golden_se_sampling.py`](../../../library/shared/fips203_se_sample/golden_se_sampling.py)
- 行 16–20 设备向量全链：[`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)
- KeyGen 全链：[`pass-fix-f203-alg13-device-keygen-k4`](../../pass-fix-f203-alg13-device-keygen-k4/) / [`exp-fips203-mlkem-pke-keygen-k4`](../../../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/)

## 冻结原因

| 类别 | 说明 |
|------|------|
| **任务完成** | Host 标量全链路 golden（行 8–15 + 行 16–20）正确性验证已完成。 |
| **不再作活跃基线** | 向量集成与 KeyGen 已迁至 vec-k4-v2 / presample / keygen exp。 |
| **胶水已迁出** | `scripts/golden_se_sampling.py` → `library/shared/fips203_se_sample/`。 |

**勿 fork、勿抄码、勿跑 CI。**
