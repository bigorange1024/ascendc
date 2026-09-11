# EN03-encrypt-host-skel · STATUS

> DAG：`D-EXP-EN03`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；NTT/INTT golden 亦 match；桩段不对拍）

---

## 1. 目标

Encrypt 形 Host 五段编排（同进程同 session）：

| 段 | kernel | 类型 |
|----|--------|------|
| L1 Prep | `enc_prep_stub` | AIV 轻桩（CPU=`AIV_ONLY`；SIM=`MIX` 占位、AIC 立即 return） |
| L2 NTT | `mmad_custom` + `M4_ntt` | 真积木（自 EN02） |
| L3 Matvec | `enc_matvec_stub` | AIV 有界 Vec MAC（`Muls`+`Adds`） |
| L4 INTT | `mmad_custom` + `M4_intt` | 真积木（独立 launch） |
| L5 Pack | `enc_pack_stub` | AIV 轻桩（`ShiftRight` 外形） |

## 2. 相对 EN02 的改动

| 路径 | 说明 |
|------|------|
| 复制 EN02 积木 | `mmad_custom` / AIV/AIC / 矩阵脚本；**未改** EN01/EN02 |
| `enc_*_stub.cpp` + `enc_aiv_stub_common.hpp` | 新建三桩；禁 CrossCore / GATE 4/8 |
| `main.cpp` | 五段 Host launch；SIM 不 recreate stream |
| `scripts/gen_data.py` | 桩输入 + EN02 式 NTT/INTT 造数 |
| `run.sh` | 五段预算 CPU=300 / SIM=1200；禁 npu |

## 3. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**1.435s**；L1–L5 均完成；NTT/INTT golden **match** |
| SIM | 0 | wall≈**4.429s**；Total tick **29824**；五段完成；golden **match**；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/EN03-cpu.log`、`EN03-sim.log`。

## 4. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h \
  enc_prep_stub.cpp enc_matvec_stub.cpp enc_pack_stub.cpp enc_aiv_stub_common.hpp \
  --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0**（未否决） |
| 非红线 | SYNC-09×3（作者包 `PIPE_ALL`×2 + matvec 连续 barrier 性能级） |
| json | `graph-tests/enc_cann_ntt/EN03-encrypt-host-skel/sync_audit.json` |

## 5. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；MIX `blockDim=1`；桩 `blockDim=1`；禁 `-r npu`。

## 6. 一条教训

Host 五段粘性可用：桩核 CPU 用真 `AIV_ONLY`，SIM 用无握手 MIX 占位（AIC 立即 return）即可挂在 NTT/INTT 两侧而不触发 GATE 4/8 空等。
