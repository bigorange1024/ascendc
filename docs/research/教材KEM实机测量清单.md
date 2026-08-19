# 教材 KEM 实机测量清单（2026-08-19 落盘）

**性质**：借入 910B4 实机测 **KEM** 性能 / profiling 的算子名单与口径。  
**不是** PKE 单算子清单；**不测** KEM KeyGen correctness（其内嵌 PKE 来自 stable Alg.13，不是 correctness PKE）。

口头曾称「16 个」：examples 树内 KEM 脚本对齐是 **16 档**（stable 1024×4 + incubating 1024/768/512 各×4）。教材对照另加 **2 档** 1024 correctness Encaps/Decaps → 脚本一并改写共 **18 档**。测量填表默认跑下面 **表 A（14 档）**。

权威入口：[`scripts/npu_kem_textbook_perf.sh`](../../scripts/npu_kem_textbook_perf.sh)。分卡：stable→**1**、examples→**2**、ascendc-tests→**3**（[`npu_device_map.sh`](../../scripts/npu_device_map.sh)）。跑前 `unset ASCEND_DEVICE_ID`。

---

## 表 A — 教材填表默认 14 档

| # | 组 | 卡 | SIM/NPU launch（约） | 路径 |
|---|----|----|----------------------|------|
| 1 | 1024 C/B KeyGen | 1 | 2 | `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4` |
| 2 | 1024 C/B Encaps | 1 | 2 | `…/stable-fips203-mlkem-kem-encaps-k4` |
| 3 | 1024 C/B Decaps | 1 | 3 | `…/stable-fips203-mlkem-kem-decaps-k4` |
| 4 | 1024 C/B Decaps-ct | 1 | 3 | `…/stable-fips203-mlkem-kem-decaps-ct-k4` |
| 5 | 768 KeyGen | 2 | 2 | `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-keygen-k3` |
| 6 | 768 Encaps | 2 | 2 | `…/exp-fips203-mlkem-kem-encaps-k3` |
| 7 | 768 Decaps | 2 | 3 | `…/exp-fips203-mlkem-kem-decaps-k3` |
| 8 | 768 Decaps-ct | 2 | 3 | `…/exp-fips203-mlkem-kem-decaps-ct-k3` |
| 9 | 512 KeyGen | 2 | 2 | `examples/incubating/ml-kem/ml-kem-512/exp-fips203-mlkem-kem-keygen-k2` |
| 10 | 512 Encaps | 2 | 2 | `…/exp-fips203-mlkem-kem-encaps-k2` |
| 11 | 512 Decaps | 2 | 3 | `…/exp-fips203-mlkem-kem-decaps-k2` |
| 12 | 512 Decaps-ct | 2 | 3 | `…/exp-fips203-mlkem-kem-decaps-ct-k2` |
| 13 | 1024 A Encaps correctness | 3 | 多段 G5 | `ascendc-tests/frozen/frozen-fix-f203-alg20-kem-encaps-correctness-k4` |
| 14 | 1024 A Decaps correctness | 3 | 多段 G5 | `…/frozen-fix-f203-alg21-kem-decaps-correctness-k4` |

**不测**：`frozen-fix-f203-alg19-kem-keygen-correctness-k4`；任何 PKE 单算子；liboqs 交叉（实机无 thirdparty 亦可跑 golden python）。

1024 incubating KEM 四档是 stable 副本，**不进表 A**；`run.sh` 已按同一套 npu/msprof 壳对齐，需要时可 `TEXTBOOK_INCLUDE_1024_INCUBATING=1`。

建议顺序：卡 1 的 1–4 → 卡 2 的 5–12 → 卡 3 的 13–14。Encaps/Decaps（含 `-ct`、correctness）有 `l18_l19` 卡死史：timeout 124 后**同卡勿连环重跑**。

---

## 表 B — examples KEM 脚本对齐 16 档

表 A 的 1–12，外加：

| 路径 |
|------|
| `examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4` |
| `…/exp-fips203-mlkem-kem-encaps-k4` |
| `…/exp-fips203-mlkem-kem-decaps-k4` |
| `…/exp-fips203-mlkem-kem-decaps-ct-k4` |

---

## 实机怎么跑（默认即出性能 + profiling）

**推荐（一次搬码测全，含 l18 E1 诊断 + 本表 14 档）**：[`docs/engineering/实机一次搬码验收清单.md`](../engineering/实机一次搬码验收清单.md) → `bash scripts/npu_kem_one_trip.sh`

仅跑本表 14 档 msprof（不含 E1 trace / PKE / roundtrip）：

```bash
unset ASCEND_DEVICE_ID
# 搬码后先看分卡（无卡也可）
NPU_SUITE_DRY_RUN=1 bash scripts/npu_kem_textbook_perf.sh

# 实机：默认 RUN_WITH_MSPROF=1、MSPROF_MODE=app（整进程一次，覆盖全部 KernelLaunch）
SKIP_L18_RISK=0 bash scripts/npu_kem_textbook_perf.sh

# 单档（与套件同一口径）
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
RUN_WITH_MSPROF=1 MSPROF_MODE=app bash run.sh -r npu -v Ascend910B4
python3 "${REPO_ROOT}/scripts/npu_msprof_summarize.py" .
```

---

## 多 launch 测准口径（不要信终端一行 duration）

KEM 一条算子在 device 上会 **KernelLaunch 多次**（Encaps≈2，Decaps≈3，correctness G5 更多）。`msprof op --launch-count` 是「重复打同一个 op」模型，**不能**代表整条 KEM。

本轮同时给三层，填表时按优先级：

| 优先级 | 来源 | 含义 |
|--------|------|------|
| **1 设备真值** | `prof_npu/<bin>/` 下 `kernel_details*.csv` 各行 Task Duration **求和**；脚本打印 `[msprof_kernel]` / `[msprof_kernel_total]` | 每个实际 kernel 的 AI Core 时间；多 launch 各占一行 |
| **2 Host 逐 launch** | stdout `[npu_launch]` + `output/npu_launch_metrics.jsonl` | `KernelLaunch` 后 `aclrtSynchronizeStream` 的墙钟（含 ACL）；与 csv 对照 |
| **3 进程墙钟** | `[wall_sec]` / `output/run_metrics.txt` | kernel 段进程时间（含 host 准备/D2H，**不含** cmake/gen_data） |

Profiling 产物：`MSPROF_MODE=app` 对**真实进程跑一遍**做应用级采集（不是把第一条 kernel 重放 8 次）。目录：`<用例>/prof_npu/<bin>/`（内含 `OPPROF_*`）。汇总：`python3 scripts/npu_msprof_summarize.py <用例>`。

correctness G5 有的路径会连续 launch 再一次 sync：host JSONL 可能是「一批」；**逐 kernel 仍以 csv 为准**。
