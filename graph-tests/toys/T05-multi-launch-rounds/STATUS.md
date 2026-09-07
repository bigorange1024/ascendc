# STATUS — T05-multi-launch-rounds

**结果：PASS**

| 项 | 值 |
|----|-----|
| 目标 | Host **串行 launch 2 次**；每轮 NTT 1/3 + 生产 GATE 4/8 + INTT 复用 1/3（禁 5/7） |
| launch 次数 | **2**（round-1: 110→trace→Sync；round-2: 120→trace→Sync→199） |
| CPU | PASS，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈2.0s |
| SIM | PASS，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈19.6s，tick 98518 |
| trace_seen (CPU) | r1: `110 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307`；r2: `120 201 301 401 402 … 307`；`199` |
| trace_seen (SIM) | r1: `110 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307`（**缺 401**，X13）；r2: `120 201 301 401 402 … 307`；`199` |
| INTT flag | 复用 **1/3**（`ST_AIV_SPLIT`/`ST_AIV_PACK`）；**未使用 5/7** |
| 体量 | T03 轻量壳（`AivLightWorkload::kRounds=4`）；非 T04×10 |
| 禁令 | 无 Wait 中 SyncAll、无 SoftSync、未抄旧 Encrypt |
| stray dump | 无（已收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/T05-cpu.log` · `/opt/cursor/artifacts/T05-sim.log` |

## 时序确认

- **Host**：round-1 `110`→launch→Sync→打印 trace；round-2 `120`→launch→Sync→打印 trace→`199`
- **每轮设备 FSM**（同 T03）：NTT 1/3 → GATE 4/8（AIC 先 WAIT4）→ INTT 1/3
- SIM 一次通过（SIM 次数 1/2）；2 轮均完成，无 Hang/Timeout

## 反馈块

```
T05: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.33 (SIM kernel 19.6s；含编译首跑 ~26s)
launches: 2
trace_seen: 110 [r1 device 201-307] 120 [r2 device 201-307] 199 (SIM r1 缺 401/X13)
notes: Host 串行 2 launch；每轮全 FSM NTT→GATE→INTT(1/3)；轻量 T03 壳；无 5/7/SyncAll/SoftSync；多 launch 不挂
```
