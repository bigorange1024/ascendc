# STATUS — toy-e19-early-entry-trace

| 项 | 状态 |
|----|------|
| 目的 | E19：MIX 入口（任何 CrossCore Wait 前）AIC+AIV0 打 fused-trace 风格入口槽；Host D2H 可见；`TOY_ROUNDS≥8` SIM |
| 图谱 | `D-exp-e19`；支撑 EARLY 空 TRACE：日后 NPU 区分 **H-E3**（入口标也无）vs **H-E1**（入口有、业务标无） |
| CPU | **非本线门禁**（未跑） |
| SIM 默认（`TOY_ROUNDS=8`） | **PASS**：每轮 `[e19-trace] stages set=2/16 : 0 15`；Host `100/111`×8；magic `E19TOY01`/`0xE9`；kernel wall **~62.8s**（budget 600）；用例根无 stray dump |
| 入口槽 | **15**=AIV0 Wait 前直写；**0**=AIC Wait 前自写 + AIV0 在 `Wait(1)` 后桥写（SIM 上 AIC→GM Host 不可见） |
| CrossCore | flag **1**（AIC→AIV0 入口宣告）+ **4**（SET4）；modeId=2；无 Cube / 无 SyncAll-in-Wait |
| 禁令 | 无 Encrypt 业务抄码；未改 Encaps/stable/图谱；禁并行 SIM；未上机；未 commit/push |

验收命令（SIM only；串行）：

```bash
cd graph_tests/toys/toy-e19-early-entry-trace
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

日志副本：`/opt/cursor/artifacts/e19-default-sim.log`；用例内 `output/host_trace.log`。
