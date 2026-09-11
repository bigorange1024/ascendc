# EN08-wired-sticky-rounds · STATUS

> DAG：`D-EXP-EN08`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM；同 session 粘性 R=4 整链不挂；第 1 轮贯通 golden match）

---

## 1. 目标

在 EN07 贯通五段上，同进程同 session **整链重复 R 轮**（默认 **4**，`EN08_ROUNDS` 可覆盖）：

```text
for r in 1..R:
  Prep → NTT → Matvec → INTT → Pack
```

粘性：不 recreate stream / 不重 aclInit；轮间可换 σ/nonce/Â/v，仍段间贯通。

## 2. 相对 EN07 的改动

| 路径 | 说明 |
|------|------|
| 复制 EN07 壳 | 核文件未改语义；**未改** EN01–07 |
| `main.cpp` | 外层 R 轮循环；缓冲/stream 只建一次；r≥2 Mutate σ/Â/v + nonce++ |
| `run.sh` | 禁 npu；`EN08_ROUNDS`；预算 CPU 600 / SIM 2400；检查 `round_XX_done.txt` |
| `scripts/*` | 第 1 轮 golden 仍用贯通链；verify 仅对拍 round-1 |

## 3. R 与每轮完成情况

| 项 | 值 |
|----|-----|
| `EN08_ROUNDS` | **4**（默认） |
| round 1 | OK（nonce=0；落盘 dst_*；golden match） |
| round 2 | OK（nonce=1；输入扰动；跑完） |
| round 3 | OK（nonce=2；输入扰动；跑完） |
| round 4 | OK（nonce=3；输入扰动；跑完） |

完成标记：`output/round_01_done.txt` … `round_04_done.txt`。

## 4. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**3.599s**；R=4 全完成；round-1 golden **match** |
| SIM | 0 | wall≈**180.474s**；Total tick **1168427**；R=4 全完成；golden **match**；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/EN08-cpu.log`、`EN08-sim-summary.log`。

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
| 非红线 | SYNC-09×12 + SYNC-11×5（与 EN07 同构，性能级） |
| json | `graph-tests/enc_cann_ntt/EN08-wired-sticky-rounds/sync_audit.json` |

## 6. 已锁参数

同 EN07：`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；Prep/Matvec/Pack `K=4`；`d_u=11`、`d_v=5`；`c=1568B`；MIX/AIV `blockDim=1`；禁 `-r npu`；禁 GATE 4/8 / 融胖核。  
新增：默认 `EN08_ROUNDS=4`。

## 7. 一条教训

真积木贯通链的粘性多轮只需 **一次 session + 复用 stream/缓冲**，轮间换输入用独立 Â 缓冲避免与 NTT src 抢占；不必 recreate stream。
