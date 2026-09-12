# EE-W0b FEEDBACK — EN09 基线 SIM 复验

| 项 | 值 |
|----|-----|
| 状态 | **PASS** |
| 日期 | 2026-09-11 |
| 目录 | `graph-tests/enc_cann_ntt/EN09-samplentt-device/` |
| 禁 | `-r npu`（未跑） |

## 命令与结果

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | 摘要 |
|------|------|------|
| CPU | 0 | 六段完成；SampleNTT+贯通 golden match；wall≈1.63s |
| SIM | 0 | Total tick **755134**；golden match；stray→`sim_log/` |

日志：`/opt/cursor/artifacts/enc-encaps-sim/w0b-en09-cpu.log`、`w0b-en09-sim.log`。

## 教训

EN09 不挂骨架仍可复验；下刀 EN13 须补 Alg.14 语义（ek/m/coins、e1/e2、t̂、真 v），不能把自洽贯通当 liboqs 交叉。
