# EN05-prep-sample-realbrick · STATUS

> DAG：`D-EXP-EN05`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；Prep/NTT/Matvec/INTT golden 亦 match）

---

## 1. 目标

Encrypt 形 Host 五段编排（同进程同 session）：

| 段 | kernel | 类型 |
|----|--------|------|
| L1 Prep | `enc_prep_cbd_real` | **真** PRF(SHAKE256)+Alg.8 CBD η=2（X32 独立 AIV） |
| L2 NTT | `mmad_custom` + `M4_ntt` | 真积木（自 EN04） |
| L3 Matvec | `enc_matvec_real` | 真 NTT 域 4×4×1 内积 |
| L4 INTT | `mmad_custom` + `M4_intt` | 真积木（独立 launch） |
| L5 Pack | `enc_pack_stub` | AIV 轻桩（X32） |

## 2. Prep 覆盖范围（本刀）

| 项 | 范围 |
|----|------|
| **必做** | Alg.8 **CBD η=2** × **K=4** poly（`y[4,256]` int32）；设备侧 **PRF = SHAKE256(σ‖N)**（shared `shake_xof_kernel`） |
| **σ 来源** | Host：`SEED_D`→d→G→σ 写入 `input/sigma.bin`；设备只读 σ |
| **SampleNTT** | **本刀不做**；Â 仍 Host 喂 `a_hat.bin`（Matvec） |
| **→NTT 布局** | Prep 输出行主序 `[k=4,n=256]`，与 cann-ntt NTT src **同构**；Host 写 `prep_ntt_layout.bin` 标明可 memcpy 喂 L2；**本刀 L2 仍用独立 `src_ntt.bin` 对拍**（无转置转接） |
| **禁抄** | Encrypt prep / alg14 / ER / 探针整文件；契约参考 shared golden + CBD 笔记 |

## 3. 相对 EN04 的改动

| 路径 | 说明 |
|------|------|
| 复制 EN04 壳 | NTT/INTT/Matvec/Pack；**未改** EN01–04 |
| `enc_prep_cbd_real.cpp` | 替换 `enc_prep_stub`；SHAKE + CBD 重写 |
| `main.cpp` / `gen_data.py` / `verify_result.py` | L1 接 σ + soft Prep golden |
| `cmake/*` | 增加 `shake_xof_kernel` / `keccak_f1600_kernel` include |
| `run.sh` | 禁 npu；`FIPS203_PRF_BACKEND=shake256` |

## 4. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**1.428s**；L1–L5 完成；Prep/NTT/Matvec/INTT golden **match** |
| SIM | 0 | wall≈**34.118s**；Total tick **246253**；五段完成；golden **match**；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/EN05-cpu.log`、`EN05-sim.log`。

## 5. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h \
  enc_prep_cbd_real.cpp enc_matvec_real.cpp enc_pack_stub.cpp enc_aiv_stub_common.hpp \
  --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0**（未否决） |
| 非红线 | SYNC-09×10 + SYNC-11×3（性能级：作者包/`PIPE_ALL`、Prep/Matvec barrier 密度） |
| json | `graph-tests/enc_cann_ntt/EN05-prep-sample-realbrick/sync_audit.json` |

## 6. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；Prep/Matvec `K=4`；`SEED_D=20260619`；`PREP_NONCE0=0`；PRF=SHAKE256；MIX `blockDim=1`；AIV 段 `blockDim=1`；禁 `-r npu`。

## 7. 一条教训

Prep 用独立 AIV（X32）+ shared SHAKE 契约重写 CBD 即可夹在五段壳上不挂；输出按 NTT 行主序落盘后 Host 转接只需 memcpy，无需抄探针/Encrypt prep。
