# EN14 FEEDBACK — Encrypt × liboqs sticky SIM（SIM-only）

| 项 | 值 |
|----|-----|
| 状态 | **PASS** |
| 日期 | 2026-09-11 |
| 目录 | `graph-tests/enc_cann_ntt/EN14-encrypt-cross-sticky/` |
| 禁 | `-r npu`（未跑） |

## 命令与结果

```bash
cd graph-tests/enc_cann_ntt/EN14-encrypt-cross-sticky
EN14_ROUNDS=8 bash run.sh -r cpu -v Ascend910B4
EN14_ROUNDS=8 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | wall | tick | 每轮 c vs liboqs |
|------|------|------|------|------------------|
| CPU | 0 | 9.573s | — | **max=0** ×8 |
| SIM | 0 | 1046.520s | **6455278** | **max=0** ×8 |

sync_audit 红线：**0**（核未相对 EN13 改动）。  
日志：`/opt/cursor/artifacts/enc-encaps-sim/en14-cpu.log`、`en14-sim.log`。

## 架构要点

- 核来自 EN13；编排参考 EN12 sticky（同 session 不 recreate stream）。  
- 每轮独立 fixture：`SEED_D+(r-1)*10007` → `input/rXX/`。  
- gen_data 先 Host≡liboqs 再接线；verify 硬门禁覆盖全部 R 轮。

## 教训

粘性多轮交叉须**预生成每轮权威 fixture**，不可在设备侧原地 mutate 种子却无对应 liboqs 期望。
