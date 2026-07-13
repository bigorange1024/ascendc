# frozen-fix-merged-kyber-ntt256-limb6-poly2-s123

> **已冻结**（2026-06-12）：merged_kyber 遗产探针；**本目录为路线全集**（原 `ascendc-tests/pass-merged-kyber-mix-ntt256` 已并入此处）。

poly2 全链路：batch Split + 2×Mmad + batch Merge（单 TPipe）。

| 张量 | 形状 |
|------|------|
| src | `[2,256]` int32 |
| dst / golden | `[2,256]` int32（NTT） |

| 组件 | 位置 |
|------|------|
| kernel | `mmad_custom.cpp`、`aic_func.hpp`、`aiv_func.hpp`、`ntt_vec.hpp` |
| golden | `scripts/ntt_sim_kyber.py` + `library/shared/merged_kyber_fixed_poly.py` |

| 模式 | 状态 |
|------|------|
| gen_data | ✓ `ntt_test01 == ntt_forward` |
| CPU  | 待复测（自包含 CMake，不依赖 `ascendc-tests/pass-merged-kyber-mix-ntt256`） |

```bash
bash run.sh -r cpu -v Ascend910B4
python3 scripts/verify_result.py
```
