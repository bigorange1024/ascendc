# STATUS — T06-gate-real-brick

**结果：PASS**

| 项 | 值 |
|----|-----|
| 目标 | 单 launch：NTT 1/3 + 生产 GATE（AIC 先 WAIT4，AIV **真 Vec MAC**）+ INTT 复用 1/3（禁 5/7） |
| CPU | PASS，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈1.4s |
| SIM | PASS，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈4.4s，tick 26094 |
| **brick 类型** | **有界 int32 Vec MAC**（`AivGateRealBrickMac`） |
| **brick 形状** | 每 AIV：`a[64]`、`b[64]`、`acc[64]` int32；**8 轮**外循环；每轮 `Muls(b,scale)→Mul(prod,a,b)→Add(acc,prod)` |
| **元素次** | 64×8=**512 向量乘加/AIV**；GM↔UB DataCopy；**非** T04 标量 SetValue 假循环（禁 X14） |
| vs T04 | T04=40 轮×256 int8 标量 +1（假体量）；T06=8 轮真 `Mul/Add/Muls` 向量 intrinsic |
| trace_seen (CPU) | `101 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| trace_seen (SIM) | `101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（**缺 401**，X13 噪声） |
| INTT flag | 复用 **1/3**；**未使用 5/7** |
| 禁令 | 无 Wait 中 SyncAll、无 SoftSync、未抄旧 Encrypt、无 X14 空转加码 |
| stray dump | 无（已收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/T06-cpu.log` · `/opt/cursor/artifacts/T06-sim.log` |

## 时序确认

- **NTT**：AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
- **GATE**：AIC SET(3) 后 TRACE(403)+WAIT(4)；AIV **真 Vec MAC** → SET(4)；AIC SET(8)；AIV WAIT(8)
- **INTT**：AIC SET(8) 后 WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
- SIM 一次通过，无 Hang/Timeout；SIM 次数 1/2；kernel wall <5min

## 反馈块

```
T06: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.07 (SIM kernel 4.4s；含编译首跑 ~11s)
brick: Vec MAC int32 a/b/acc[64]×2 AIV, 8 rounds Mul+Add+Muls, 512 MAC ops/AIV
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199 (SIM 缺 401)
notes: 生产 GATE 时序不变；真向量积木 SIM 不挂；INTT 仍复用 1/3；tick 26094 vs T04 298706（真 MAC 比假循环×40 更快）
```
