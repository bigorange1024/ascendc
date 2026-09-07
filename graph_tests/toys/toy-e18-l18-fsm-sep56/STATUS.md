# STATUS — toy-e18-l18-fsm-sep56

| 项 | 状态 |
|----|------|
| 目的 | E18：相对 E17 **单因子** — 同序伪 NTT 1/3 → GATE 4/8 → 伪 INTT 改为独立 **5/6**（非复用 1/3）；Host `1xx`；≥8 轮 SIM |
| 图谱 | `D-exp-e18`；支撑 `F-encrypt-gap-inventory` G1+G2 / H-sep56 离线可活 |
| CPU | **非本线门禁**（未跑） |
| SIM 默认（`TOY_ROUNDS=8`） | **PASS**：Host `100/111` 各 8；设备 AIC `400…423` + 双 AIV `5xx` 全序；magic `E18TOY01`/`0xE8`；kernel wall **~100.8s**（budget 600；整次 `WALL_SEC≈108`） |
| CrossCore | flag **1/3**（伪 NTT）+ **4/8**（GATE）+ **5/6**（伪 INTT 独立）；modeId=2；无 Cube / 无 SyncAll-in-Wait |
| 相对 E17 差分 | **仅** INTT 段 `ST_INTT_AIV_SPLIT=5` / `ST_INTT_AIV_PACK=6` + TRACE 注释/magic/`E18` 标识；NTT/GATE 序不变 |
| sync_audit | `/opt/cursor/artifacts/e18-sync-audit.json`：SYNC-03 假阳性（`FsmWait`/`FsmSet` 包装函数同侧）；SYNC-09 性能提示；**无真死等红线** |
| 禁令 | 无 Encrypt 业务抄码；未改 E17；禁并行 SIM；未上机；未 commit/push |

验收命令（SIM only；串行）：

```bash
cd graph_tests/toys/toy-e18-l18-fsm-sep56
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp --check all --format json > /opt/cursor/artifacts/e18-sync-audit.json
```

日志副本：`/opt/cursor/artifacts/e18-default-sim.log`；用例内 `output/host_trace.log`。
