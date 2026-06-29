# KeyGen SIM Debug 指南（给家里 Agent）

**任务**：`exp-mlkem-f203-pke-keygen-k4` + 探针 `pass-fix-f203-alg13-device-keygen-k4` 的 prep 融合路径；当前 **workaround 已 KAT PASS**，目标是 **恢复双 AIV 并行 Â** 且 SIM golden 仍 PASS。

**先读**：`README.md` → `qa/2026-06/2026-06-26-标量探针冻结.md` §KeyGen SIM prep → `.cursor/rules/ascendc-development.mdc`

---

## 1. 问题一句话

SIM 上 prep **并行**写 `a_hat` shard1（GM 行 8–15）错 → `ek_pke` poly2–3 错；`dk_pke`/`src`/`ρ` 仍对。

---

## 2. 当前 workaround（勿误删）

`f203_keygen_prep_ub.hpp`（example + 探针各一份）：

- `F203_AHAT16_BLOCK_DIM==2` → **sub0/block0 串行** shard 0+1
- sub1 空转 Â，段末 `F203_PREP_PIPE_ALL()`（P-04）

Launch 差异（**不可混抄 entry**）：

| | 探针 | Example |
|---|---|---|
| Host blockDim | `2`（`kPrepBlockDim`） | `1`（`kPrepHostLaunchBlockDim`） |
| 分片 ID | `GetBlockIdx()` | `GetSubBlockIdx()` |

---

## 3. 复现与定位（按序跑）

```bash
# 环境
source ~/ascendc/scripts/env.sh   # 或 CANN setenv

# 固定种子 — 必跑 CPU + SIM
cd examples/incubating/exp-mlkem-f203-pke-keygen-k4
SEED_D=20260619 python3 scripts/gen_data.py
SEED_D=20260619 KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 SEED_D=20260619 KEYGEN_VERIFY=1 bash run.sh -r sim -v Ascend910B4

# 中间量（SIM 已支持 D2H）
SEED_D=20260619 KEYGEN_DEBUG_DUMP=1 bash run.sh -r sim -v Ascend910B4
python3 - <<'PY'
import numpy as np
from pathlib import Path
a=np.fromfile("output/debug/after_prep_a_hat.bin",dtype=np.int32).reshape(16,256)
g=np.fromfile("output/golden_a_hat.bin",dtype=np.int32).reshape(16,256)
for r in range(16):
    d=int(np.abs(a[r]-g[r]).max())
    if d: print("row",r,"maxdiff",d)
PY

# 全链 KAT（慢；SIM 1 轮 ~6min）
KAT_CPU_COUNT=5 KAT_SIM_COUNT=1 bash kat_liboqs_vs_ascendc.sh
# 日志：output/kat_liboqs_vs_ascendc.log

# 探针分段
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
KAT_SEED_D=20260619 KAT_RUN_MODE=sim python3 scripts/kat_liboqs_staged.py
```

**判读**：

| 检查 | 通过 | 失败时看 |
|---|---|---|
| `a_hat` 行 0–7 | diff=0 | prep shard0 / Alg7 |
| `a_hat` 行 8–15 | diff=0 | **并行 shard1 或 UB 复用** |
| `ek_pke` byte 768+ | 与 golden 一致 | `a_hat` 或 mmad 行18 |
| `dk_pke` | 全对 | 正常（不依赖 `a_hat`） |

---

## 4. 修并行 Â 的建议顺序

1. **基线对照**：`pass-fix-f203-alg13-lines3-7-a-hat-k4` 在 `F203_AHAT16_BLOCK_DIM=2` 下 SIM PASS（独立探针，无 PRF/CBD 复用）。
2. **临时回退并行**（仅 debug 分支）：在 `f203_keygen_prep_ub.hpp` 恢复 `#else` 里 per-shard `BuildAHat16ShardWithUb(..., shardIdx, ...)`，用 §3 dump 确认行 8–15 再现。
3. **读同步契约**：`f203_keygen_prep_sync.hpp`、`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/PIPE_SYNC_EVAL.md`（Phase 4 窄化曾 `a_hat≈3272` FAIL — 勿未证 SIM 删 barrier）。
4. ** diff 焦点**：prep 内 Â 结束后 shakeXBuf/aHatQue/scratchBuf 交 PRF/CBD 复用前的 `P-02 PIPE_ALL`；a_hat16 热循环 `f203_a_hat16_ub.hpp` 的 GM `DataCopy` 前后 barrier（A-06 等）。
5. **勿改 locked 参数**：`blockDim=1`+双 AIV、`K=4`、2 launch、生产 I/O 除非与用户确认。
6. **验收门槛**：`KEYGEN_VERIFY=1` CPU+SIM + `KAT_CPU_COUNT=5 KAT_SIM_COUNT=1`；声称 PASS 前必须 **SIM 非仅 CPU**。

---

## 5. 关键路径

```
examples/incubating/exp-mlkem-f203-pke-keygen-k4/
  f203_keygen_prep_ub.hpp      # Â 串行 workaround
  f203_keygen_prep_entry.cpp   # GetSubBlockIdx
  f203_keygen_prep_layout.h    # kPrepHostLaunchBlockDim=1
  main_keygen.cpp              # 2 launch；KEYGEN_DEBUG_DUMP SIM D2H
  cmake/cpu_lib_keygen.cmake   # F203_CBD_BLOCK_DIM=1 等

ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/   # 探针镜像 + PIPE_SYNC_EVAL.md
ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/f203_a_hat16_ub.hpp
ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/mmad_custom.cpp  # FuseEkPke
```

---

## 6. 红旗

- 只跑 `-r cpu` 就报完成
- `KEYGEN_SKIP_REBUILD=1` 混用 cpu/sim 产物（曾出现 `libcpudebug` 加载失败）
- 把探针 `GetBlockIdx()` entry 原样拷到 example（blockDim 语义不同）
- 从 `frozen/` 抄实现
