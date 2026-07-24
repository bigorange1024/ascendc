# 2026-07-21 — 连续 SIM `tcache` 对照矩阵与归类

## 背景

办公室 WSL 跑 [`scripts/stable_kem_liboqs_roundtrip.sh`](../../scripts/stable_kem_liboqs_roundtrip.sh) 曾在 **SIM KeyGen→Encaps→立刻 Decaps** 的 Decaps Phase-D 出现：

`tcache_thread_shutdown(): unaligned tcache chunk detected` → Aborted（fixture `output/stable_kem_liboqs_rt/20260720_185052_42894/`）。  
同 fixture 单独 Decaps SIM 绿；次日端到端复跑亦曾全绿。计划：对照矩阵 C0–C5 各 ≥3 次。

## 失败瞬间特征（原失败 log，无可用 host core）

| 项 | 观察 |
|----|------|
| 进程模型 | KeyGen / Encaps / Decaps 各为独立 `run.sh` 进程（非同进程多算子） |
| 崩点 | Decaps **Phase-D** 已 `Model Start`；约 **130s** 后 `tcache_thread_shutdown`；**无** Decaps `Total tick`；**未**进入 Phase-E |
| 信号类 | glibc **线程退出时 tcache 完整性检查失败**（堆元数据已坏）→ host abort；**非** golden/`max≠0` |
| core | 矩阵跑全程 `ulimit -c unlimited`；**未**收获可 gdb 的 host `core`（仅有 CAModel `sim_log/core*.dump` 指令迹，非进程 core） |
| 与历史差异 | 不同于「双库 func_key / 单 session 输出污染」（错结果）；本现象是 **进程崩** |

Decaps Phase-D/E 各自 `aclInit`…`aclFinalize`（见 `main_kem_decaps_phase_{d,e}_run.cpp`）；默认 `decaps_1session`。

## 对照矩阵（定点 fixture `185052_42894`）

驱动：`/tmp/tcache_sim_bisect.sh`；结果：[`output/tcache_sim_bisect/summary.tsv`](../../output/tcache_sim_bisect/summary.tsv)（约 5.5h 墙钟）。

| ID | 步骤 | n | ok | tcache |
|----|------|---|----|--------|
| C0 | 仅 Decaps SIM | 3 | 3 | **0** |
| C1 | Encaps→立刻 Decaps | 3 | 3 | **0** |
| C2 | KeyGen→立刻 Decaps | 3 | 3 | **0** |
| C3 | KeyGen→Encaps→Decaps | 3 | 3 | **0** |
| C4 | KeyGen→Encaps→sleep30→Decaps | 3 | 3 | **0** |
| C5 | KeyGen→Encaps→清 CAMODEL env + 清 Decaps SIM build→Decaps | 3 | 3 | **0** |

合计：**18/18 PASS，tcache 0/18**。本矩阵未再现原失败。

## 判决

1. **正确性**：算法/接线不背锅（CPU 绿 + 单独/矩阵 Decaps 绿 + tick 与 Cloud 同档）。
2. **根因归类**：**CAModel / glibc 宿主机级偶发堆损坏**，在 Decaps Phase-D 模型运行中或收尾触发；与 roundtrip 串联 **相关但非必现**（历史 1 次 fail + 多次 pass；矩阵 C3 亦 0/3）。
3. **触发条件**：未能用 C0–C5 在本机 **统计显著** 分离「仅连续 SIM」vs「Decaps 自身」——偶发率低于 1/18 量级；原失败仍落在「连续 SIM 后立刻 Decaps」叙事，但 **非稳定复现 bug**。
4. **缓解**：C4/C5 **无**可度量降发（基线已 0）→ **不**改 [`stable_kem_liboqs_roundtrip.sh`](../../scripts/stable_kem_liboqs_roundtrip.sh)（禁止为消偶发改 stable 算子）。

## 操作口径（维持）

```bash
set -o pipefail
bash scripts/stable_kem_liboqs_roundtrip.sh 2>&1 | tee /tmp/stable_kem_rt.log
echo EXIT:$?
```

再遇 `tcache`：同 fixture 单独 Decaps SIM；绿则记环境偶发，勿当算法回归。


## 家里 WSL 对照（同日合入；另一 Agent）

环境：WSL2，Mem **3.7 Gi**（available ≈2.6–2.8 Gi）。日志原路径 `/tmp/tcache_rt/`。

| 组 | 条件 | EXIT | 结果 |
|----|------|------|------|
| EXP1 | 全链 CPU+SIM（pipefail+tee） | **0** | **PASS** |
| EXP2 | `CAMODEL_SKIP_ADX_WORK_PATH=1` + `SKIP_CPU=1` | **0** | **PASS** |
| EXP1b | 基线再跑 `SKIP_CPU=1` | **0** | **PASS** |
| EXP3 | Phase 间 sleep **45s** | **0** | **PASS** |
| EXP4a | 只 KeyGen→Decaps | **0** | **PASS** |
| EXP4b | 只 Encaps→Decaps | **0** | **PASS** |

全部无 `tcache`/`Aborted`；无 host core。ADX skip 与否均绿 → **不能**据此排除 dump 路径。与公司侧 C0–C5 **一致**：**本日两侧均未复现**；维持「CAModel/glibc 宿主机偶发、非算法回归、不改 roundtrip/stable」。
