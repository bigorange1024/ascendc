# ⛔ STATUS — frozen-fix-f203-alg13-device-presample-a-hat-k4（只读）

> **2026-06-28 已冻结**。继任：[`pass-fix-f203-alg7-sample-ntt-k4`](../../pass-fix-f203-alg7-sample-ntt-k4/) · [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../../pass-fix-f203-alg13-lines3-7-a-hat-k4/) · [`pass-fix-f203-alg13-lines8-15-se-k4`](../../pass-fix-f203-alg13-lines8-15-se-k4/)。见 [`FROZEN.md`](FROZEN.md)。

| 阶段 | CPU | SIM | 说明 |
|------|-----|-----|------|
| Phase 1 标量 A + 8–15 回归 | ✓ | ✓ **719237** | `VERIFY_STAGE=all` |
| A-v1 shake_vec | ✓ | ✓ **918301** | batch SHAKE128 |
| A-v2 ~ A-v3 | ✓ | ✓ **715537** ~ **881627** | 见 SIM_BENCHMARK |
| A-v4a / A-v4b | ✓ | ✓ | 负优化；默认 `SE_A_HAT_REJ=scalar` |
| A-v5 d1/d2 UB POC | — | — | **待开工** |
| 链式 / vec-k4-v3 | — | — | 阶段 2 |

**默认路径**：`SE_A_HAT_STAGE=shake_vec`，`SE_A_HAT_REJ=scalar`。

**交接注意**：若目录缺少 `run.sh`、`main.cpp`、`f203_a_hat_*.hpp` 等，需从 backup/git 恢复后再跑 benchmark。
