# ⛔ 已冻结（2026-06-26）

**原路径**：`ascendc-tests/fix-f203-alg13-se-device-scalar-k4/`  
**继任**：[`pass-fix-f203-alg13-lines8-15-se-k4`](../../pass-fix-f203-alg13-lines8-15-se-k4/)（设备行 8–15，默认 `F203_SE_VECTOR_V3`）

## 冻结原因

| 类别 | 说明 |
|------|------|
| **任务完成** | 阶段一a 标量正确性：`SEED_D`→`src`（行 8–15）CPU/SIM golden **PASS**（SIM **178188** tick，2026-06-22）。 |
| **向量版已接班** | 设备 PRF batch + CBD 向量路径在 [`pass-fix-f203-alg13-lines8-15-se-k4`](../../pass-fix-f203-alg13-lines8-15-se-k4/) **PASS**；本探针仅作历史标量对照，不再维护。 |
| **共享 golden 已迁出** | `golden_se_sampling.py` 迁入 [`library/shared/fips203_se_sample/`](../../../library/shared/fips203_se_sample/golden_se_sampling.py)。 |

**勿 fork、勿抄码、勿跑 CI。**
