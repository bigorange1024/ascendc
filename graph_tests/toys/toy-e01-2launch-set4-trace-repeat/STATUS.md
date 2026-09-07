# STATUS — toy-e01-2launch-set4-trace-repeat

| 项 | 状态 |
|----|------|
| 目的 | E01：新目录 2-launch + Wait(4)/SET(4) + 数字 TRACE；同进程 8 轮 SIM；OMIT_SET4⇒124 |
| 图谱 | `D-exp-e01`；引用 `F-omit-set4-sim124` / `F-set4-ok-sim` |
| CPU | **非本线门禁**（未跑） |
| SIM 默认（`OMIT_SET4=0` `TOY_ROUNDS=8`） | **PASS**：Host `100/101/110/111` 各 8；设备 `400/401/402` + `500/502`/`510/512`；magic `E01TOY01`/`0xE1`；kernel wall **~75.2s** |
| SIM OMIT（`OMIT_SET4=1` `TOY_ROUNDS=1` budget=60） | **HANG rc=124**：Host 见 `100/101/110` 无 `111`；wall **~60.1s** |
| CrossCore | 仅 flag **4**（双 AIV SET ↔ AIC Wait）；无 Cube / GATE 8 / SoftSync |
| 禁令 | 无 Encrypt 业务抄码；无双 Cube/GATE alone 复测；禁并行 SIM |

验收命令（SIM only；串行）：

```bash
cd graph_tests/toys/toy-e01-2launch-set4-trace-repeat
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
KERNEL_COMPUTE_BUDGET_SEC=60 TOY_ROUNDS=1 OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

日志副本：`/opt/cursor/artifacts/e01-default-sim.log`、`e01-omit-set4-sim.log`；用例内 `output/host_trace.log`（末次跑为 OMIT）。
