# STATUS — toy-e17-l18-fsm-reuse13

| 项 | 状态 |
|----|------|
| 目的 | E17：单 launch MIX stub 复现 l18 CrossCore 全序（伪 NTT 1/3 → GATE 4/8 → 伪 INTT **复用 1/3**）；Host `1xx`；≥8 轮 SIM |
| 图谱 | `D-exp-e17`；支撑 `F-encrypt-gap-inventory` G1+G2 / H-reuse 离线可活 |
| CPU | **非本线门禁**（未跑） |
| SIM 默认（`TOY_ROUNDS=8`） | **PASS**：Host `100/111` 各 8；设备 AIC `400…423` + 双 AIV `5xx` 全序；magic `E17TOY01`/`0xE7`；kernel wall **~80.7s**（budget 600） |
| CrossCore | flag **1/3**（伪 NTT + 伪 INTT 复用）+ **4/8**（GATE）；modeId=2；无 Cube / 无 SyncAll-in-Wait |
| sync_audit | `/opt/cursor/artifacts/e17-sync-audit.json`：SYNC-03 假阳性（`FsmWait`/`FsmSet` 包装函数同侧）；SYNC-09 性能提示；**无真死等红线** |
| 禁令 | 无 Encrypt 业务抄码；未做 E18 / OMIT / 仅 GATE；禁并行 SIM；未上机 |

验收命令（SIM only；串行）：

```bash
cd graph_tests/toys/toy-e17-l18-fsm-reuse13
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp --check all --format json > /opt/cursor/artifacts/e17-sync-audit.json
```

日志副本：`/opt/cursor/artifacts/e17-default-sim.log`；用例内 `output/host_trace.log`。
