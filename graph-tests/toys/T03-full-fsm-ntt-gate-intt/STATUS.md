# STATUS — T03-full-fsm-ntt-gate-intt

**结果：PASS**

| 项 | 值 |
|----|-----|
| 目标 | 单 launch：NTT 1/3 + 生产 GATE 4/8 + INTT 复用 1/3（禁 5/7） |
| CPU | PASS，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈1.2s |
| SIM | PASS，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈9.5s，tick 49387 |
| trace_seen (CPU) | `101 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| trace_seen (SIM) | `101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（**缺 401**，X13 噪声） |
| INTT flag | 复用 **1/3**（`ST_AIV_SPLIT`/`ST_AIV_PACK`）；**未使用 5/7** |
| 禁令 | 无 Wait 中 SyncAll、无 SoftSync、未抄旧 Encrypt |
| stray dump | 无（已收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/T03-cpu.log` · `/opt/cursor/artifacts/T03-sim.log` |

## 时序确认

- **NTT**：AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
- **GATE**：AIC SET(3) 后 TRACE(403)+WAIT(4)；AIV 轻体量 → SET(4)；AIC SET(8)；AIV WAIT(8)
- **INTT**：AIC SET(8) 后 WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
- SIM 一次通过，无 Hang/Timeout；SIM 次数 1/2

## 反馈块

```
T03: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.3 (SIM kernel 9.5s；含编译首跑 ~17s)
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199 (SIM 缺 401)
notes: INTT 复用 flag 1/3（第二轮 SET/WAIT 1/3）；绝对未用 5/7；三段 TRACE 可区分（NTT 201-303 / GATE 403-305 / INTT 206-307）；单 launch 全 FSM 闭环
```
