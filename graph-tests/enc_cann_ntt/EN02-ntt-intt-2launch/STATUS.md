# EN02-ntt-intt-2launch · STATUS

> DAG：`D-EXP-EN02`  
> 日期：2026-09-08  
> 结论：**PASS-NOHANG**（CPU + SIM 均不挂；两段 golden 对拍亦过）

---

## 1. 目标

在迁入的 cann-ntt 积木上验证 **Host 连续两段 launch 不挂**：

1. Launch1：**正向** NTT（`q=3329`，`M4_ntt`）  
2. Launch2：**逆向** INTT（`M4_intt` + 相应输入；同构 MIX 核）  
3. 同进程、同 device session 连续跑完

## 2. 相对 EN01 的改动

| 路径 | 说明 |
|------|------|
| 整目录复制 `EN01-kem256-ntt-port/` | 起点；**未改** EN01 |
| `main.cpp` | Host 两段 launch：先 NTT 再 INTT；SIM 不 recreate stream |
| `scripts/gen_inverse_matrix.py` | 只读改编 cann-ntt `mlkem_inverse_ntt` + dense 矩阵；M4 打包对齐作者包 |
| `scripts/gen_data.py` | 写出 `src_ntt/M4_ntt/golden_ntt` 与 `src_intt/M4_intt/golden_intt` |
| `scripts/verify_result.py` | 两段分别对拍 |
| `run.sh` | 双 launch 预算 CPU=180 / SIM=900；禁 npu |

设备侧核（`mmad_custom` / AIV / AIC）与 EN01 同构，仅注释标明双 launch 共用。

## 3. 验收命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | wall≈**1.027s**；Launch1+2 均完成；NTT/INTT golden **match** |
| SIM | 0 | wall≈**3.618s**；Total tick **20685**；两段完成；golden **match**；stray→`sim_log/`；根无 `core*.dump` |

日志：`/opt/cursor/artifacts/EN02-cpu.log`、`EN02-sim.log`。

## 4. sync_audit

```bash
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp ntt_vec.hpp basic.hpp tiling.h --format json
```

| 项 | 值 |
|----|-----|
| 红线 | **0**（未否决） |
| 非红线 | SYNC-09×2（`PipeBarrier<PIPE_ALL>` 粒度，性能级，保留作者握手） |
| json | `graph-tests/enc_cann_ntt/EN02-ntt-intt-2launch/sync_audit.json` |

## 5. 已锁参数

`NTT_N=256`、`NTT_Q=3329`、`NTT_REF=kyber`、`NTT_BENCH=4`；`blockDim=1` MIX；禁 `-r npu`。

## 6. 一条教训

Host 双 launch 粘性：同 session 换 `M4`+输入即可复用短 MIX 核做 INTT；inverse 矩阵语义取 cann-ntt UT，digit 打包须对齐作者包 M4（非 UT 的 16 块 parity BAT）。
