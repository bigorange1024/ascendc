# EN06-pack-compress-realbrick · STATUS

> DAG：`D-EXP-EN06`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；Prep/NTT/Matvec/INTT/Pack golden 亦 match）

---

## 1. 目标

Encrypt 形 Host 五段编排（同进程同 session）：

| 段 | kernel | 类型 |
|----|--------|------|
| L1 Prep | `enc_prep_cbd_real` | 真 PRF+CBD η=2（沿用 EN05） |
| L2 NTT | `mmad_custom` + `M4_ntt` | 真积木 |
| L3 Matvec | `enc_matvec_real` | 真 NTT 域 4×4×1 内积 |
| L4 INTT | `mmad_custom` + `M4_intt` | 真积木 |
| L5 Pack | `enc_pack_compress_real` | **真** Compress₁₁/₅ + ByteEncode → `c`（X32 独立 AIV） |

## 2. Pack 覆盖范围（本刀）

| 项 | 范围 |
|----|------|
| **必做** | ML-KEM-1024 全密文外形：`c = ByteEncode₁₁(Compress₁₁(u)) ‖ ByteEncode₅(Compress₅(v))` |
| **几何** | `u[K=4,N=256]`、`v[N]` int32 ∈ `[0,q)`；`c` **1568B**（4×352 + 160） |
| **Compress** | 统一整数舍入 `C=⌊2^37/q⌋`（笔记 P-UCOMP-1）；`d_u=11`、`d_v=5` |
| **ByteEncode** | Alg.5 标量逐组 pack（VEC=1）；禁 VEC=2 Gather |
| **输入** | Host 独立造数 `u_pack.bin` / `v_pack.bin`（不接 INTT 输出；须 STATUS 声明） |
| **禁抄** | Encrypt / alg14 / ER / pack 探针整文件；契约参考笔记 + shared 公式 |

## 3. 相对 EN05 的改动

| 路径 | 说明 |
|------|------|
| 复制 EN05 壳 | Prep/NTT/Matvec/INTT；**未改** EN01–05 |
| `enc_pack_compress_real.cpp` | 替换 `enc_pack_stub`；Compress+ByteEncode 按笔记重写 |
| `main.cpp` / `gen_data.py` / `verify_result.py` | L5 接 u/v → `dst_pack.bin` / `golden_pack.bin` |
| `run.sh` | 禁 npu；标签 EN06 |

## 4. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**1.430s**；L1–L5 完成；Prep/NTT/Matvec/INTT/Pack golden **match** |
| SIM | 0 | wall≈**45.352s**；Total tick **296054**；五段完成；golden **match**；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/EN06-cpu.log`、`EN06-sim.log`。

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
| json | `graph-tests/enc_cann_ntt/EN06-pack-compress-realbrick/sync_audit.json` |

## 6. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；Prep/Matvec/Pack `K=4`；`d_u=11`、`d_v=5`；`c=1568B`；MIX `blockDim=1`；AIV 段 `blockDim=1`；禁 `-r npu`。

## 7. 一条教训

Pack 用独立 AIV（X32）+ 统一整数 Compress + Alg.5 标量 ByteEncode 重写即可挂五段壳且对拍全密文；勿抄 Encrypt/alg14 pack 核，输入可独立造数须在 STATUS 声明。
