# TRACE.md — toy-e19-early-entry-trace

对照知识库 §6（三位十进制）+ fused-trace 槽（int32[16] Host D2H）。

## 入口槽语义（EARLY；非 Encaps 业务号）

| 槽 | 谁写 | 何时 | Host 可见性 |
|----|------|------|-------------|
| **15** | AIV0 标量 | **任何** CrossCore Wait **之前** | SIM/NPU：AIV→GM 直写可见 |
| **0** | AIC 标量（NPU 直写）+ AIV0 桥写 | AIC：Wait 前自写并 `Set(1)`；AIV0：`Wait(1)` 后写同槽 | SIM：AIC→GM 对 Host **不可见**（标量/DataCopy 均验过）；靠 AIV0 桥写让 D2H 见 `0`；NPU 预期可直读 AIC 写 |

未采用槽 14：与 15 同 32B 半区，且 AIC 直写在 SIM 上 Host 永不见（偶发/稳定失败）。

## Host / 设备数字 TRACE

| 号 | 谁 | 含义 |
|----|----|------|
| 100 | Host | 将单 launch MIX |
| 111 | Host | Sync 返回且入口槽验收通过 |
| 400 | AIC | 入口 / 将写槽 0 |
| 404 | AIC | 已尝试写槽 0 |
| 405 | AIC | 已 Set(1) 宣告入口 |
| 401 | AIC | 将 Wait(4) |
| 402 | AIC | Wait(4) 返回 |
| 500 | AIV0 | 入口 / 将写槽 15 |
| 504 | AIV0 | 已写槽 15 |
| 505 | AIV0 | Wait(1) 后已桥写槽 0 |
| 502 | AIV0 | 已 Set(4) |
| 510 | AIV1 | 入口 |
| 512 | AIV1 | 已 Set(4) |

## Host 日志行

```text
[e19-trace] round=N stages set=P/16 : 0 15
```

**判读**：默认 `TOY_ROUNDS=8` 应见 N×`100/111`，且每轮 `[e19-trace]` 含槽 **0** 与 **15**（`stages set≥2`）。
