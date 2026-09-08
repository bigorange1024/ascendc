# EN09-samplentt-device · STATUS

> DAG：`D-EXP-EN09`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；SampleNTTÂ + Prep→NTT→Matvec→INTT→Pack 贯通 golden 亦 match）

---

## 1. 目标

在 EN07 贯通五段上接入 **设备侧 Alg.7 SampleNTT**，使 Matvec 的 Â **来自本刀设备输出**（禁止 Host 随机 Â 冒充）。

```text
L0 SampleNTT(ρ→Â[4,4,256]) → L1 Prep(σ→y) → L2 NTT → L3 Matvec(Â_dev,ŷ) → L4 INTT → L5 Pack
```

## 2. SampleNTT 覆盖范围（本刀声明）

| 项 | 范围 |
|----|------|
| **覆盖** | **全矩阵** Â：`k=4` → **16 poly**（`Â[i,j]=SampleNTT(ρ‖byte(j)‖byte(i))`）→ `â[256]` |
| **XOF** | shared SHAKE128；固定 squeeze **672B**（笔记契约；不做 lazy tail） |
| **rej** | 标量解交织 + 边扫边填（生产默认路径；未用向量 compact） |
| **ρ 来源** | Host：`SEED_D`→d→`G` 前 32B 写 `input/rho.bin`；设备只读 ρ |
| **喂入 Matvec** | **是** — `aHatDevice` / CPU `aHat` 直喂 `enc_matvec_real`；**不再**读 Host `a_hat.bin` |
| **Launch** | **独立 AIV**（X32）；未并入 Prep launch；禁融 NTT MIX |
| **禁抄** | Encrypt prep / alg7 探针整文件 / ER；按笔记 + shared SHAKE 重写 |

## 3. 段间 I/O（相对 EN07）

| 段 | kernel | 输入来源 | 输出 |
|----|--------|----------|------|
| L0 SampleNTT | `enc_samplentt_real` | `rho.bin`（Host） | `Â` → `dst_a_hat` **→ Matvec** |
| L1–L5 | 同 EN07 | 同 EN07 贯通 | 同 EN07；Matvec Â 改设备 |

**仍 Host**：γ、v（噪声形）；σ（Prep）。

## 4. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**1.632s**；L0–L5 完成；SampleNTT+贯通 golden **match** |
| SIM | 0 | wall≈**128.879s**；Total tick **755040**；六段完成；golden **match**；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/EN09-cpu.log`、`EN09-sim.log`。

## 5. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h \
  enc_samplentt_real.cpp enc_prep_cbd_real.cpp enc_matvec_real.cpp enc_pack_compress_real.cpp \
  enc_aiv_stub_common.hpp \
  --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0**（未否决） |
| 非红线 | SYNC-09 / SYNC-11 性能级（`PIPE_ALL`、EnQue 握手密度；含 SampleNTT 段） |
| json | `graph-tests/enc_cann_ntt/EN09-samplentt-device/sync_audit.json` |

## 6. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；K=4；XOF=672B；`SEED_D=20260619`；MIX `blockDim=1`；AIV 段 `blockDim=1`；R=1；禁 `-r npu`；禁 GATE 4/8。

## 7. 一条教训

设备 SampleNTT 用独立 AIV + shared SHAKE128(672) 标量 rej 即可产出全 Â 并直喂 Matvec；串行 16 poly 控 UB，SIM 体量↑（tick≈755k）但不挂。
