# TRACE 编号分区 — RB-T01-mix-ntt13-handshake

> 设备写魔数到 `ws+TRACE`；Host 在 launch 前 / `SynchronizeStream` 后写槽 0/7 并打印。

| 槽 | 分区 | 时机 | 魔数 | 备注 |
|----|------|------|------|------|
| 0 | **Host** | launch 前 | `0x484F5354` | 硬验收 |
| 1 | **AIV0** | SET(1) 前 | `0xA1010001` | 硬验收（SET1） |
| 2 | **AIV1** | SET(1) 前 | `0xA1110001` | 软：CAModel 上常空 |
| 3 | **AIC** | WAIT(1) 后 | `0xC1010001` | 软：AIC 标量写 GM 常空 |
| 4 | **AIC** | Cube 完、SET(3) 前 | `0xC1030003` | 软：同上 |
| 5 | **AIV0** | WAIT(3) 后 | `0xA1030003` | 硬验收 |
| 6 | **AIV1** | WAIT(3) 后 | `0xA1130003` | 软 |
| 7 | **Host** | SynchronizeStream 后 | `0x484F5355` | 硬验收（sync 后） |

**WAIT1 / SET3 可见性**：Host 在 SET1+WAIT3 齐时打印  
`causal: SET1 seen + WAIT3 seen => AIC WAIT1 + SET3 completed`；  
另用 `mat_c.bin` 非全 0 证明 AIC 在 WAIT1→SET3 间跑过极轻 Cube。


## CrossCore

| flagId | 方向 | 含义 |
|--------|------|------|
| **1** | AIV → AIC | 向量侧就绪（双 AIV 均 SET） |
| **3** | AIC → AIV | Cube 完成（双 AIV 均 WAIT） |
| 禁 | — | **5 / 7**；SoftSync；AIC Wait 环内 SyncAll |

## 日志关键字

Host 打印行含：`AIV0_PRE_SET1` / `AIC_POST_WAIT1` / `AIC_PRE_SET3` / `HOST_POST_SYNC`（sync 后）。
