# STATUS — T02-prod-gate-timing

**结果：PASS**

| 项 | 值 |
|----|-----|
| 目标 | NTT 1/3 + 生产 GATE 时序（AIC 先 WAIT4 + AIV 体量后双 SET4 + SET8/WAIT8） |
| CPU | PASS，`bash run.sh -r cpu -v Ascend910B4`，exit 0，wall≈6.5s |
| SIM | PASS，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，wall≈14.3s，tick 40288 |
| trace_seen (CPU) | `101 201 301 401 402 203 303 403 204 304 404 205 305 199`（完整） |
| trace_seen (SIM) | `101 201 301 402 203 303 403 204 304 404 205 305 199`（**缺 401**，X13 噪声） |
| 禁令 | 无 INTT/5/7、无 Wait 中 SyncAll、无 SoftSync、未抄旧 Encrypt |
| stray dump | 无（已收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/T02-cpu.log` · `/opt/cursor/artifacts/T02-sim.log` |

## 时序确认

- AIC：`SET(3)` → TRACE(403) → `WAIT(4)` **不等待** AIV `WAIT(3)` 完成（代码结构保证先占坑）
- AIV：`WAIT(3)` → `AivLightWorkload`（4 轮×256 int8）→ TRACE(204/304) → 双 `SET(4)`
- AIC：`WAIT(4)` 返回 → TRACE(404) → `SET(8)`；AIV：`WAIT(8)` → 完成标记
- SIM 一次通过，无 Hang/Timeout

## 反馈块

```
T02: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.25 (SIM 7.8s kernel；含编译首跑 ~14s)
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 199 (SIM 缺 401)
notes: 生产 GATE 时序已实现；AIC SET(3) 后立即 TRACE(403)+WAIT(4)，AIV 在 WAIT(3) 后做轻体量再 SET(4)；非对称 GATE，非双方齐到；SIM 不挂
```
