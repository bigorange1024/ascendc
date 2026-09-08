# EN01-kem256-ntt-port · STATUS

> DAG：`D-EXP-EN01`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；golden 对拍亦过）

---

## 1. 目标

迁入 cann-ntt 作者包 **ML-KEM n=256 q=3329 正向 NTT**，形成独立 KernelLaunch 用例；主门禁 **不挂**。

## 2. 迁入文件列表

| 路径 | 来源 |
|------|------|
| `mmad_custom.cpp` | `thirdparty/cann-ntt-author-merged_dsa/mmad_custom.cpp`（加中文注释；CrossCore 语义未改） |
| `aiv_func.hpp` | 同上；修 SYNC-02：`_AivGmProbe` DataCopy→EnQue/DeQue |
| `aic_func.hpp` | 同上 |
| `ntt_vec.hpp` | 同上；修 SYNC-02：CPU_DEBUG GetValue 前 `PipeBarrier<PIPE_V>` |
| `tiling.h` / `basic.hpp` / `data_utils.h` / `main.cpp` | 同上 |
| `scripts/gen_data.py` `ntt_kyber.py` `ntt_sim_kyber.py` `ntt_reference.py` `verify_result.py` | 同上；默认锁 `NTT_Q=3329` `NTT_REF=kyber` |
| `cmake/*` `CMakeLists.txt` `run.sh` | **壳**对齐 `graph-tests/enc_related/ER03-*`（仅壳；无 ER 核） |

未改 `thirdparty/`；未搬 ACLNN；未开 INTT / GATE / Encrypt。

## 3. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | kernel wall≈**0.826s**；golden **match**（error ratio 0） |
| SIM | 0 | kernel wall≈**1.842s**；Total tick **11356**；golden **match**；stray 已收拢 `sim_log/`；用例根无 `core*.dump` |

日志副本：`/opt/cursor/artifacts/EN01-cpu.log`、`EN01-sim.log`。

## 4. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0**（初扫 3 条 SYNC-02 已修；未否决） |
| 非红线 | SYNC-09×2（`PipeBarrier<PIPE_ALL>` 粒度，性能级，保留作者握手） |
| json | `graph-tests/enc_cann_ntt/EN01-kem256-ntt-port/sync_audit.json`（及 `/opt/cursor/artifacts/EN01-sync_audit.json`） |

## 5. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；`blockDim=1` MIX AIC×1+AIV×2；禁 `-r npu`。

## 6. 一条教训

迁入作者包后 **先跑 sync_audit**：CPU_DEBUG 探针里 `DataCopy`/`Cast` 后直接 `GetValue` 会打出 SYNC-02 红线，须补 Que 或 `PipeBarrier<PIPE_V>`，不可主观否决。
