# EN12-samplentt-sticky · STATUS

> DAG：`D-EXP-EN12`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM R=16；NPU sticky R=32/64；同 session 贯通链不挂；round-1 golden match）

---

## 1. 目标

在 EN09 SampleNTT 贯通六段上，同进程同 session **整链重复 R 轮**（默认 **16**，`EN12_ROUNDS` 可覆盖）：

```text
for r in 1..R:
  L0 SampleNTT → L1 Prep → L2 NTT → L3 Matvec → L4 INTT → L5 Pack
```

粘性：不 recreate stream / 不重 aclInit；缓冲一次分配、R 轮复用；Â **每轮**由设备 SampleNTT 产出（禁止 Host Â 冒充）。

## 2. 相对 EN09 / EN08 的改动

| 路径 | 说明 |
|------|------|
| 复制 EN09 壳 | 核文件未改语义；**未改** EN01–11；**未抄** Encrypt/KEM/frozen/ER |
| `main.cpp` | 外层 R 轮；缓冲/stream 只建一次；r≥2 Mutate ρ/σ/v + nonce++；每轮重跑 SampleNTT |
| `run.sh` | `ASCENDC_CASE_SUPPORTS_NPU=1`；npu 默认 `ASCEND_DEVICE_ID=4`；`EN12_ROUNDS`；预算 CPU 1200 / SIM 7200；检查 `round_XX_done.txt` |
| `scripts/*` | 第 1 轮 golden 仍用 SampleNTT+贯通链；verify 仅对拍 round-1 |

## 3. R 与每轮完成情况

| 项 | 值 |
|----|-----|
| `EN12_ROUNDS` | **16**（默认） |
| round 1 | OK（nonce=0；落盘 dst_*；golden match） |
| round 2–16 | OK（nonce=1…15；ρ/σ/v 扰动；每轮设备 SampleNTT；跑完） |

完成标记：`output/round_01_done.txt` … `round_16_done.txt`。

## 4. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**23.04s**（kernel≈13.7s）；R=16 全完成；round-1 golden **match** |
| SIM | 0 | wall≈**2711s**（kernel≈**1881s**）；Total tick **12000090**；R=16 全完成；golden **match**；stray→`sim_log/` |
| NPU | 0 | 910B3；`aclrtSetDevice(0)`（X41）；R=32 `run.sh` wall≈**2.06s** + R=64 stress；golden **match** |

日志：`/opt/cursor/artifacts/EN12-cpu-summary.log`、`EN12-sim-summary.log`、`EN12b-npu.log`。

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
| 非红线 | SYNC-09 / SYNC-11 性能级（`PIPE_ALL`、EnQue 握手密度；含 SampleNTT）共 21 条 |
| json | `graph-tests/enc_cann_ntt/EN12-samplentt-sticky/sync_audit.json` |

## 6. 已锁参数

同 EN09：`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；K=4；XOF=672B；`SEED_D=20260619`；MIX/AIV `blockDim=1`；`d_u=11`、`d_v=5`；`c=1568B`。  
新增：默认 `EN12_ROUNDS=16`；允许 `-r npu`（默认 device **4**）。

## 7. 一条教训

SampleNTT 贯通链的粘性多轮：同 session 复用 stream/缓冲即可；轮间只 mutate ρ（再设备 SampleNTT）而非 Host 写 Â，SIM R=16 不挂（kernel≈1881s / tick≈12M）。
