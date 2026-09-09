# TRACE.md — toy-e20-postmark-tail

对照知识库 §6（三位十进制）。本实验仅打印数字，无长字符串。

| 号 | 谁 | 含义 |
|----|----|------|
| 100 | Host | 将单 launch MIX |
| 111 | Host | Sync 返回（整轮成功关键点；含 AIC 早退 + AIV 尾包） |
| 400 | AIC | 入口 / 将 Wait(1) 伪 NTT |
| 401 | AIC | Wait(1) 返回（跳过 Cube） |
| 403 | AIC | 已 Set(3) 伪 NTT |
| 410 | AIC | 将 Wait(4) GATE |
| 411 | AIC | Wait(4) 返回 |
| 418 | AIC | 已 Set(8) GATE |
| 420 | AIC | 将 Wait(1) 伪 INTT（复用） |
| 421 | AIC | Wait(1) 返回 |
| 423 | AIC | 已 Set(3) 伪 INTT；**立即 return（早退）** |
| 500 | AIV0 | 入口 / 将 Set(1) 伪 NTT |
| 501 | AIV0 | 已 Set(1) |
| 503 | AIV0 | Wait(3) 返回 |
| 504 | AIV0 | 已 Set(4) GATE |
| 508 | AIV0 | Wait(8) 返回 |
| 521 | AIV0 | 已 Set(1) 伪 INTT |
| 523 | AIV0 | Wait(3) 返回；进入 postmark |
| 540 | AIV0 | **postmark**：将进非对称尾包（heavy） |
| 541 | AIV0 | 尾包完成；写 magic |
| 510 | AIV1 | 入口 / 将 Set(1) 伪 NTT |
| 511 | AIV1 | 已 Set(1) |
| 513 | AIV1 | Wait(3) 返回 |
| 514 | AIV1 | 已 Set(4) GATE |
| 518 | AIV1 | Wait(8) 返回 |
| 531 | AIV1 | 已 Set(1) 伪 INTT |
| 533 | AIV1 | Wait(3) 返回；进入 postmark |
| 550 | AIV1 | **postmark**：将进非对称尾包（light） |
| 551 | AIV1 | 尾包完成 |

**判读**：同进程 `TOY_ROUNDS`（默认 8）应见 **N×** `100/111`；每轮设备侧应走完 AIC `400…423`（无尾包 TRACE）、双 AIV `5xx` 全序 + `540/541` 与 `550/551`。  
未做 OMIT / 真 tail_pack / NPU。
