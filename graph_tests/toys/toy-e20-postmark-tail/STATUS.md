# STATUS — toy-e20-postmark-tail

| 项 | 状态 |
|----|------|
| 目的 | E20：E17 全 Mark 序后 **AIC 早退** + 双 AIV **非对称 stub DataCopy 尾包**（无后续 CrossCore）；Host `1xx`；≥8 轮 SIM |
| 图谱 | 支撑 `J-hang-after-full-trace`（收紧「末段」可活结构）；对齐 `J-sim-not-sticky`（本 stub 亦不粘） |
| CPU | **非本线门禁**（未跑） |
| SIM 默认（`TOY_ROUNDS=8`） | **PASS**：Host `100/111` 各 8；设备 AIC `400…423` 早退；AIV0 `5xx`+`540/541`（heavy×6）；AIV1 `5xx`+`550/551`（light×2）；magic `E20TOY01`/`0xE0`；kernel wall **~110.2s**（budget 600） |
| CrossCore | flag **1/3**（伪 NTT + 伪 INTT 复用）+ **4/8**（GATE）；modeId=2；INTT 末次 Set(3) 后 AIC `return`；尾包 **无** CrossCore |
| sync_audit | `/opt/cursor/artifacts/e20-sync-audit.json`：SYNC-03 假阳性（`FsmWait`/`FsmSet` 包装）；SYNC-09 性能提示；**无真死等红线** |
| 禁令 | 无 Encrypt / `f203_tail_pack_ops` / frozen 抄码；禁并行 SIM；未上机 |

验收命令（SIM only；串行）：

```bash
cd graph_tests/toys/toy-e20-postmark-tail
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp --check all --format json > /opt/cursor/artifacts/e20-sync-audit.json
```

日志副本：`/opt/cursor/artifacts/e20-default-sim.log`；用例内 `output/host_trace.log`。
