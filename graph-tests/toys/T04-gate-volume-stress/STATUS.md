# STATUS — T04-gate-volume-stress

**结果：PASS**

| 项 | 值 |
|----|-----|
| 目标 | 单 launch：NTT 1/3 + 生产 GATE（AIC 先 WAIT4，AIV **40 轮**体量）+ INTT 复用 1/3（禁 5/7） |
| CPU | PASS，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈1.4s |
| SIM | PASS，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈60.3s，tick 298706 |
| 体量（相对 T03） | `kRounds` **40**（T03=4，×10）；`kWorkPerAiv` **256** int8（不变）；每 AIV touch **10240**（T03=1024） |
| trace_seen (CPU) | `101 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| trace_seen (SIM) | `101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（**缺 401**，X13 噪声） |
| INTT flag | 复用 **1/3**（`ST_AIV_SPLIT`/`ST_AIV_PACK`）；**未使用 5/7** |
| 禁令 | 无 Wait 中 SyncAll、无 SoftSync、未抄旧 Encrypt |
| stray dump | 无（已收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/T04-cpu.log` · `/opt/cursor/artifacts/T04-sim.log` |

## 时序确认

- **NTT**：AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
- **GATE**：AIC SET(3) 后 TRACE(403)+WAIT(4)；AIV **40 轮**体量 → SET(4)；AIC SET(8)；AIV WAIT(8)
- **INTT**：AIC SET(8) 后 WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
- SIM 一次通过，无 Hang/Timeout；SIM 次数 1/2；kernel wall <5min 目标

## 反馈块

```
T04: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~1.1 (SIM kernel 60.3s；含编译首跑 ~67s)
volume: kRounds=40 (T03=4), kWorkPerAiv=256, touch/AIV=10240 (T03=1024)
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199 (SIM 缺 401)
notes: 生产 GATE 时序不变；AIV 体量×10 后 SIM 仍不挂；INTT 仍复用 1/3；三段 TRACE 可区分；tick 298706 vs T03 49387
```
