# EN04-matvec-realbrick · STATUS

> DAG：`D-EXP-EN04`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；NTT/Matvec/INTT golden 亦 match）

---

## 1. 目标

Encrypt 形 Host 五段编排（同进程同 session）：

| 段 | kernel | 类型 |
|----|--------|------|
| L1 Prep | `enc_prep_stub` | AIV 轻桩（X32） |
| L2 NTT | `mmad_custom` + `M4_ntt` | 真积木（自 EN03） |
| L3 Matvec | `enc_matvec_real` | **真** NTT 域 4×4×1 内积（Alg.11/12） |
| L4 INTT | `mmad_custom` + `M4_intt` | 真积木（独立 launch） |
| L5 Pack | `enc_pack_stub` | AIV 轻桩（X32） |

## 2. 相对 EN03 的改动

| 路径 | 说明 |
|------|------|
| 复制 EN03 壳 | NTT/INTT/Prep/Pack；**未改** EN01–03 |
| `enc_matvec_real.cpp` | 按笔记重写 P-inner-1 + paired basemul；`f203_mod_q`；禁抄 Encrypt/ER/探针整文件 |
| `main.cpp` / `gen_data.py` / `verify_result.py` | L3 接 `a_hat`/`s_hat`/`gammas` + soft golden |
| `cmake/*` | 增加 `library/shared` include |
| `run.sh` | 禁 npu；软对拍含 Matvec |

## 3. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**1.431s**；L1–L5 完成；NTT/Matvec/INTT golden **match** |
| SIM | 0 | wall≈**27.359s**；Total tick **194125**；五段完成；golden **match**；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/EN04-cpu.log`、`EN04-sim.log`（摘要 `EN04-sim-summary.log`）。

## 4. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h \
  enc_prep_stub.cpp enc_matvec_real.cpp enc_pack_stub.cpp enc_aiv_stub_common.hpp \
  --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0**（未否决） |
| 非红线 | SYNC-09×6 + SYNC-11×3（性能级：作者包 `PIPE_ALL`、matvec 握手/barrier 密度） |
| json | `graph-tests/enc_cann_ntt/EN04-matvec-realbrick/sync_audit.json` |

## 5. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；Matvec `K=4`；MIX `blockDim=1`；AIV 段 `blockDim=1`；禁 `-r npu`。

## 6. 一条教训

真 Matvec 用独立 AIV（X32）+ 笔记契约行主序重写即可夹在 NTT/INTT 之间不挂；正确性可用标量 Alg.11/12 + `f203_mod_q` 对拍，无需粘贴探针核。
