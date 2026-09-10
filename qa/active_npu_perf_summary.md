# 活跃用例 NPU 算子性能一览

| 项 | 内容 |
|----|------|
| **刷新** | 2026-09-10（Launch 压缩重建六档；CANNLab / Ascend910B3 / `ASCEND_DEVICE_ID=0`） |
| **平台** | Ascend910B3 真机；cannbot **`ops-profiling`**（`msprof_profile_run.sh --warm-up=3` + `msprof_perf_summary.py`） |
| **口径** | 各目录 **默认配置** 下 PipeUtilization `op_summary` 各 kernel **Task Duration** 求和（µs）；分 kernel 在备注展开；未测为 `n/a` |
| **来源优先级** | `STATUS.md` 性能节 → 本表 → ops-profiling `docs/perf/round_*` / 远端 PROF |
| **范围** | 当前已上板登记的 `graph-tests/RB-*` 重建档；**不含** `frozen/`；**非** SIM Total tick（SIM 见 [`active_sim_regress_summary.md`](active_sim_regress_summary.md)，本表不改该文件） |
| **维护** | 手工登记（非 CI dump）；上板验收通过后刷新对应行 |
| **禁止** | 用 host `[wall_sec]` / 自造 wall 填本表 |

远端套件（本轮）：`/mnt/workspace/launch-reduce-logs/ops-profiling-rebuild-20260910-160409/`（`index.csv` 六档 `summary_rc=0`）。

---

## graph-tests 重建 — Launch 压缩终态（2026-09-10）

> 路径根：[`graph-tests/`](../graph-tests/)。Host launch 数见备注；Σ = 各 kernel Task Duration 之和。

| ID | 目录 | NPU Σ Task Duration (µs) | 备注 | 来源 |
|----|------|--------------------------|------|------|
| **K07** | [`RB-K07-pke-2launch`](../graph-tests/kg_related/RB-K07-pke-2launch/) | **1126.58** | Host **2**：prep **448.16** + `kg_ntt_dot_encode` **678.42**；cube_util 2.83% | STATUS / ops-profiling 2026-09-10 |
| **K09** | [`RB-K09-kem-2launch`](../graph-tests/kg_related/RB-K09-kem-2launch/) | **1227.12** | Host **2**：prep **447.16** + `kg_ntt_dot_encode` **779.96**；cube_util 2.47% | STATUS / ops-profiling 2026-09-10 |
| **T19** | [`RB-T19-encrypt-ek-decode-full`](../graph-tests/enc_related/RB-T19-encrypt-ek-decode-full/) | **1238.78** | Host **2**：prep **19.68** + compute **1219.10**；对照测（非 launch 压缩刀） | STATUS / ops-profiling 2026-09-10 |
| **T23** | [`RB-T23-encaps-liboqs-cross`](../graph-tests/enc_related/RB-T23-encaps-liboqs-cross/) | **1283.54** | Host **2**：prep **63.44** + compute **1220.10**；对照测 | STATUS / ops-profiling 2026-09-10 |
| **D09** | [`RB-D09-decrypt-1launch`](../graph-tests/dec_related/RB-D09-decrypt-1launch/) | **455.74** | Host **1**：`d09_decrypt_fused` **455.74**；cube_util 4.44% | STATUS / ops-profiling 2026-09-10 |
| **T30** | [`RB-T30-decaps-2launch`](../graph-tests/enc_related/RB-T30-decaps-2launch/) | **1719.22** | Host **2**：`t30_dec` **455.12** + `t30_enc` **1264.10** | STATUS / ops-profiling 2026-09-10 |

### 与 stable 同角色量级对照（参考，非门禁）

| 角色 | 重建（本表 Σ µs） | Host launch | stable 参考 launch |
|------|-------------------|-------------|-------------------|
| PKE KeyGen | K07 **1127** | 2 | 2 |
| KEM KeyGen | K09 **1227** | 2 | 2 |
| PKE Encrypt | T19 **1239** | 2 | 2 |
| KEM Encaps | T23 **1284** | 2 | 2 |
| PKE Decrypt | D09 **456** | 1 | 1 |
| KEM Decaps | T30 **1719** | 2 | 3 |

---

## 采集命令（登记前）

```bash
# 用例目录；bin 名以该用例为准
bash thirdparty/cannbot-skills/ops/ops-profiling/scripts/msprof_profile_run.sh \
  --warm-up=3 --output=./msprof_output -- ./out/<bin>
python3 thirdparty/cannbot-skills/ops/ops-profiling/scripts/msprof_perf_summary.py \
  ./msprof_output/PROF_GROUP_* .
# 填表：Σ = PipeUtilization op_summary 各 Op Task Duration 之和
```
