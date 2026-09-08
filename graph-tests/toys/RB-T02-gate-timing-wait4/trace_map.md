# TRACE 编号分区 — RB-T02-gate-timing-wait4

> 设备写魔数到 `ws+TRACE`；Host 在 launch 前 / `SynchronizeStream` 后写槽 0/7 并打印。

| 槽 | 分区 | 时机 | 魔数 | 备注 |
|----|------|------|------|------|
| 0 | **Host** | launch 前 | `0x484F5354` | 硬（单槽） |
| 1 | **AIV0** | SET(4) 前 | `0xA1040004` | 与槽 2 成对硬：任一侧命中（SET4/GATE） |
| 2 | **AIV1** | SET(4) 前 | `0xA1140004` | 与槽 1 成对；空侧 soft WARN |
| 3 | **AIC** | WAIT(4) 后 | `0xC1040004` | 软：AIC 标量写 GM 常空 |
| 4 | **AIC** | Cube 完、SET(3) 前 | `0xC1030003` | 软：同上 |
| 5 | **AIV0** | WAIT(3) 后 | `0xA1030003` | 与槽 6 成对硬 |
| 6 | **AIV1** | WAIT(3) 后 | `0xA1130003` | 与槽 5 成对 |
| 7 | **Host** | SynchronizeStream 后 | `0x484F5355` | 硬（单槽） |
| 8 | **AIV0** | SET(1) 前 | `0xA1010001` | 软（NTT 段） |
| 9 | **AIV1** | SET(1) 前 | `0xA1110001` | 软 |
| 10 | **AIC** | WAIT(1) 后 | `0xC1010001` | 软 |

**验收**：AIV 逻辑事件取 **AIV0|AIV1 成对槽任一魔数匹配**（NPU TRACE 可能落 AIV1，勿硬绑 AIV0 / KB X7/X9）。

**WAIT4 / SET3 可见性**：Host 在 SET4+WAIT3 齐时打印  
`causal: SET4 seen + WAIT3 seen => AIC WAIT4 + Cube + WAIT1 + SET3 completed`；  
并打印 `gate: AIC entered Wait(4) before AIV Set(4) (expected; woken by Set)`。  
另用 `mat_c.bin` 非全 0 证明 AIC 在 WAIT4→SET3 间跑过极轻 Cube。

## CrossCore

| flagId | 方向 | 含义 |
|--------|------|------|
| **4** | AIV → AIC | GATE：向量侧就绪（双 AIV 均 SET）；AIC **先** Wait |
| **1** | AIV → AIC | NTT 段就绪（双 AIV 均 SET） |
| **3** | AIC → AIV | Cube+握手完成（双 AIV 均 WAIT） |
| 禁 | — | **5 / 7**；SoftSync；AIC Wait 环内 SyncAll |

## 时序（预期）

```text
AIC:  Wait(4) → Wait(1) → Cube → Set(3)
AIV:  Set(4)  → Set(1) → Wait(3)
```

AIC 在 AIV Set(4) **之前**进入 Wait(4) 属预期（靠 Set 唤醒）；对照 KB §X1。  
极轻 Cube 置于 Wait(1)→Set(3) 之间（与 T01 同构），仍在 GATE Wait(4) 之后。

## 日志关键字

Host 打印行含：`AIV0_PRE_SET4` / `AIC_POST_WAIT4` / `AIV0_PRE_SET1` / `AIC_PRE_SET3` / `HOST_POST_SYNC`。
