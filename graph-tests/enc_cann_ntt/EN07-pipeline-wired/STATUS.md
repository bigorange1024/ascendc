# EN07-pipeline-wired · STATUS

> DAG：`D-EXP-EN07`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；Prep→NTT→Matvec→INTT→Pack 贯通链 golden 亦 match）

---

## 1. 目标

在 EN06 五段真积木上，把 **GM 数据流贯通**（禁止每段独立随机造互不相关主路径数据）：

```text
Prep(CBD→y) → NTT(y→ŷ) → Matvec(Â,ŷ→t̂) → INTT(t̂→u) → Pack(u,v→c)
```

## 2. 段间 I/O 来源（贯通表）

| 段 | kernel | 输入来源 | 输出 | Host 仍喂 |
|----|--------|----------|------|-----------|
| L1 Prep | `enc_prep_cbd_real` | `sigma.bin`（Host） | `y[4,256]` → `dst_prep` | σ |
| L2 NTT | `mmad_custom` + `M4_ntt` | **L1 的 `y`（memcpy/D2D）** | `ŷ` → `dst_ntt` | `M4_ntt` |
| L3 Matvec | `enc_matvec_real` | **L2 的 `ŷ` 作 ŝ**；Â/γ Host | `t̂` → `dst_matvec` | `a_hat`、`gammas` |
| L4 INTT | `mmad_custom` + `M4_intt` | **L3 的 `t̂`（memcpy/D2D）** | `u` → `dst_intt` | `M4_intt` |
| L5 Pack | `enc_pack_compress_real` | **L4 的 `u`**；`v` Host | `c[1568B]` → `dst_pack` | `v_pack`（噪声形） |

**未贯通（本刀声明）**：设备 SampleNTT（Â 仍 Host）；Encrypt 全噪声/消息链（`v` 仍 Host 独立造数）。  
**禁止冒充**：不再读 `src_ntt.bin` / `s_hat.bin` / `src_intt.bin` / `u_pack.bin` 作为主路径输入。

## 3. 相对 EN06 的改动

| 路径 | 说明 |
|------|------|
| 复制 EN06 壳 | 核文件未改语义；**未改** EN01–06 |
| `main.cpp` | 段间 memcpy/D2D 贯通；五段 launch 序不变 |
| `scripts/gen_data.py` | 贯通链 golden：ŷ=NTT(y)、t̂=Matvec(Â,ŷ)、u=INTT(t̂)、c=Pack(u,v) |
| `scripts/verify_result.py` | 贯通链软对拍（失败不否决不挂） |
| `run.sh` | 禁 npu；标签 EN07 |

## 4. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**1.427s**；L1–L5 完成；贯通 golden **match** |
| SIM | 0 | wall≈**45.421s**；Total tick **296064**；五段完成；golden **match**；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/EN07-cpu.log`、`EN07-sim.log`。

## 5. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h \
  enc_prep_cbd_real.cpp enc_matvec_real.cpp enc_pack_compress_real.cpp enc_aiv_stub_common.hpp \
  --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0**（未否决） |
| 非红线 | SYNC-09×12 + SYNC-11×5（性能级：`PIPE_ALL`、Pack/Matvec EnQue 握手密度） |
| json | `graph-tests/enc_cann_ntt/EN07-pipeline-wired/sync_audit.json` |

## 6. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`（=K）；Prep/Matvec/Pack `K=4`；`d_u=11`、`d_v=5`；`c=1568B`；MIX `blockDim=1`；AIV 段 `blockDim=1`；禁 `-r npu`；禁 GATE 4/8 / 融胖核。

## 7. 一条教训

段间贯通只需同布局 memcpy/D2D（Prep 行主序已与 NTT src 同构）；golden 须按同一条链重算，切勿再为 L2/L3/L4/L5 独立随机造主路径数据冒充连通。
