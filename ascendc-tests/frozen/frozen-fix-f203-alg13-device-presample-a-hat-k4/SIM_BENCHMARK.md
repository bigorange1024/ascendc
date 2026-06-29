# SIM_BENCHMARK — frozen-fix-f203-alg13-device-presample-a-hat-k4（只读）

| 项 | 值 |
|----|-----|
| **SoC** | Ascend910B4 CaModel |
| **SEED_D** | 20260619（默认）；复测可用 14 |
| **Launch** | `blockDim=1` |
| **管线** | G → A → P（SHAKE256 batch=8）→ C（P1b-single） |

---

## 全段 tick

| 日期 | `SE_A_HAT_STAGE` | `SE_A_HAT_REJ` | Phase A | 总 tick | 备注 |
|------|------------------|----------------|---------|---------|------|
| 2026-06-23 | scalar | — | PermuteChain | **719237** | Phase 1 基线 |
| 2026-06-23 | shake_vec | scalar | A-v1+v2 路径 | **918301** | batch XOF + GM 标量 rej |
| 2026-06-23 | shake_vec | — | A-v2 无 SetValue | **715537** | 历史最快 shake_vec |
| 2026-06-23 | shake_vec | scalar | A-v3 + GM 栈 | **881627** | **现行 scalar 基线** |
| 2026-06-23 | shake_vec | vec_a | A-v4a | **960762** | +9.0% vs 881627 |
| 2026-06-23 | shake_vec | vec_b | A-v4b LUT | **1004273** | +13.9% vs 881627 |

母探针 V3 仅行 8–15：**133153** tick。

---

## 解读

1. Phase A 约占全段 **~81%**（586k / 719k 差分）。
2. **881627 vs 715537**：现行管线多了 bulk DataCopy + 每 poly GM 读 504B + 栈 rej；v4 实验必须与 **881627** 比。
3. A-v4a/b：**功能 PASS**，SIM **负优化**；默认保持 `SE_A_HAT_REJ=scalar`。

---

## 复现

```bash
cd ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4
SE_A_HAT_REJ=scalar bash run.sh -r sim -v Ascend910B4   # 881627
SE_A_HAT_REJ=vec_a   bash run.sh -r sim -v Ascend910B4   # 960762
SE_A_HAT_REJ=vec_b   bash run.sh -r sim -v Ascend910B4   # 1004273
```

SIM 单次约 7–10 分钟。
