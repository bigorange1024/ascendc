# frozen-merged-kyber-ntt256-limb6 — Phase D′

> **已冻结**（2026-06-12）：6bit limb 单 poly 基线；**勿 fork**。`ascendc-tests/pass-merged-kyber-mix-ntt256` 为 **7bit 参考实现**，思想可借鉴，**不可照抄 limb 相关代码**。

| | |
|--|--|
| **CPU** | **PASS**（2026-06-27，`max_abs_diff=0`） |
| **SIM** | **PASS**（2026-06-27，`max_abs_diff=0`，tick≈13156） |

## 7bit vs 6bit 分工（必读）

| 来源 | 可借鉴 | 必须 6bit 自研 |
|------|--------|----------------|
| `ascendc-tests/pass-merged-kyber-mix-ntt256`（7bit） | MIX 状态机、ws 布局、`AicMmad`（`LoadDataWithTranspose` / Fixpipe）、Barrett 流程 | `aiv_func` Merge shift、`ntt_vec` split、`gen_data` M4 切片、`kyber_limb6.hpp` |
| 本目录 | — | mask `0x3f`，shift **6/12/18**，`LIMB_BITS=6`，golden 仍 `f@M mod q` |

**禁止**：把官方 `aiv_func.hpp` / `gen_data.py`（`& 0x7f`，shift 7/14/21）整份拷入本探针。

```bash
cd ascendc-tests/frozen/frozen-merged-kyber-ntt256-limb6
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

**自包含**（2026-06-27）：`main.cpp`、`aic_func.hpp`（仅 Stage2，与 limb 无关）、`tiling.h`、`kyber_limb6.hpp`、`aiv_func.hpp`、`ntt_vec.hpp`、`scripts/verify_result.py` 已入目录；`gen_data.py` 用 6bit M4，NTT 矩阵仍引用 `library/shared` + `ntt_sim_kyber.py`。
