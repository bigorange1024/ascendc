# TRACE.md — toy-e02-softsync-then-set4

对照知识库 §6（三位十进制）。本实验仅打印数字，无长字符串。

| 号 | 谁 | 含义 |
|----|----|------|
| 100 | Host | 将 launch L1 |
| 101 | Host | L1 Sync 返回 |
| 110 | Host | 将 launch L2 |
| 111 | Host | L2 Sync 返回（整轮成功关键点） |
| 400 | L2 AIC | L2 AIC 入口 |
| 401 | L2 AIC | 将 Wait(4) |
| 402 | L2 AIC | Wait(4) 返回（SET 配对成功） |
| 500 | L2 AIV0 | AIV0 入口 |
| 503 | L2 AIV0 | SoftSyncArrive 完成（已写哨兵） |
| 502 | L2 AIV0 | 已 SET(4) |
| 510 | L2 AIV1 | AIV1 入口 |
| 513 | L2 AIV1 | SoftSyncArrive 完成（自旋结束） |
| 512 | L2 AIV1 | 已 SET(4) |

**判读**：同进程 3 轮应见 **3×** `100/101/110/111`；设备侧每轮 L2 见 `400/401/402` 与 `500/503/502`、`510/513/512`（SIM 日志/tee 中）。  
顺序不变量：AIV 侧 **503/513 在 502/512 之前**（SoftSync → SET）。  
`OMIT_SOFTSYNC=1`：仍应绿（若绿则 SoftSync 对本骨架非必要 / weaken）；仍见 Host `111×rounds`。
