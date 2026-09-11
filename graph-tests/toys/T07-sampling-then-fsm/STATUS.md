# STATUS — T07-sampling-then-fsm

**结果：PASS**

| 项 | 值 |
|----|-----|
| 目标 | SAMPLE stub 前置 → NTT 1/3 + 生产 GATE（AIC 先 WAIT4，AIV 真 Vec MAC）+ INTT 复用 1/3（禁 5/7） |
| CPU | PASS，`bash run.sh -r cpu -v Ascend910B4`，exit 0，kernel wall≈1.6s |
| SIM | PASS，`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，exit 0，kernel wall≈7.8s，tick 42682 |
| **sampling stub** | Host：`seed.bin` 32B urandom + `ref_sha3.bin`（`hashlib.sha3_256`，同 `fips203_se_sample/golden_se_sampling.py`） |
| **设备 SAMPLE** | `AivSampleStub`：seed 8×int32 → 16×int32，**4 轮** Muls+Add 向量 mixing → **64B/AIV** 写 `SAMPLE_OUT`；再 `AivStubHashSplit` 铺 S0 |
| **GATE brick** | 同 T06：`AivGateRealBrickMac` int32[64]×8 轮 Mul+Add |
| trace_seen (CPU) | `108 101 211 311 212 312 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`（完整） |
| trace_seen (SIM) | `108 101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 211 311 212 312 199`（**缺 401**，X13 噪声；SAMPLE 四码可见） |
| INTT flag | 复用 **1/3**；**未使用 5/7** |
| 禁令 | 无 SyncAll/SoftSync；未抄 Encrypt；无 X14 空转 |
| stray dump | 无（已收拢至 `sim_log/`） |
| 日志 | `/opt/cursor/artifacts/T07-cpu.log` · `/opt/cursor/artifacts/T07-sim.log` |

## 时序确认

- **SAMPLE**：Host 108 → AIV 211/311 → mixing → 212/312 → SAMPLE_OUT→S0
- **NTT**：AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
- **GATE**：AIC TRACE(403)+WAIT(4)；AIV 真 Vec MAC → SET(4)；AIC SET(8)；AIV WAIT(8)
- **INTT**：AIC WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
- SIM 1 次通过；总墙钟 <25min

## 反馈块

```
T07: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.13 (SIM kernel 7.8s；含编译首跑 ~15s)
sample: Host seed 32B + sha3_256 ref; AivSampleStub 16×int32 4-round Muls+Add → 64B/AIV
trace_seen: 108 211 311 212 312 + NTT/GATE/INTT + 199 (SIM 缺 401)
notes: T06 FSM 壳不变；SAMPLE 前置补结构缺口；SIM 不挂；禁 5/7/X14
```
