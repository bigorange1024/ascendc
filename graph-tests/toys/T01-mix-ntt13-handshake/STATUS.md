# STATUS — T01-mix-ntt13-handshake

**结果：PASS（主控复验）**

| 项 | 值 |
|----|-----|
| 目标 | 新树最短 MIX：NTT 同构 CrossCore 1/3 + TRACE |
| CPU | PASS，trace `101 201 301 401 402 203 303 199`，wall≈1.0s |
| SIM | `SIM_DIRECT=1` PASS，trace `101 201 301 402 203 303 199`（**缺 401**），tick 10332，wall≈2.0s |
| 禁令 | 无 5/7、无 Wait 中 SyncAll、无 SoftSync、未抄旧 Encrypt |
| 日志 | `/opt/cursor/artifacts/T01-cpu-reverify.log` · `/opt/cursor/artifacts/T01-sim-reverify.log` |

## 过程教训（进 KB）

1. **环境与实验预算要拆开**：冷机装 CANN 吃掉首刀 30min → 记为 `X-ENV-COLD-CANN`；以后实验刀默认假定 CANN 已就绪，否则先单独「环境刀」。
2. **CPU 孪生 TPipe 作用域**：`TPipe` 未析构就开第二个 VECOUT pipe → abort；已修。
3. **SIM 缺 401**：AIC WAIT(1) 后 mark 在 SIM 未回显，但 402/握手完成仍在 → 归入 TRACE 可见性噪声，**不**据此判握手失败（与旧 AIC TRACE 教训同类）。

## 反馈块

```
T01: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.2 (复验；首刀 subagent 曾 BLOCKED@29min 因装 CANN)
trace_seen: 101 201 301 402 203 303 199 (SIM 缺 401)
notes: 新树脚手架可用；1/3 握手 SIM 不挂
```
