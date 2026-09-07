# TRACE.md — toy-e03-stage-skel-2launch

对照知识库 §6（三位十进制）。本实验仅打印数字，无长字符串。  
**形态**：Encrypt 骨架 stub — L1=采样打点（无 SHAKE）；L2=代数打点（假 NTT/点积/INTT）+ SET(4)；**默认无 SoftSync**。

| 号 | 谁 | 含义 |
|----|----|------|
| 100 | Host | 将 launch L1 |
| 101 | Host | L1 Sync 返回 |
| 105 | Host | μ 空操作（不写设备） |
| 110 | Host | 将 launch L2 |
| 111 | Host | L2 Sync 返回（整轮成功关键点） |
| 200 | L1 AIV0 | 进入 L1 |
| 201 | L1 AIV0 | 假 seed expand（无 SHAKE） |
| 202 | L1 AIV0 | 假 CBD/noise |
| 203 | L1 AIV0 | L1 将返回 |
| 400 | L2 AIC | L2 AIC 入口 |
| 401 | L2 AIC | 将 Wait(4) |
| 402 | L2 AIC | Wait(4) 返回（SET 配对成功） |
| 500 | L2 AIV0 | AIV0 入口 |
| 520 | L2 AIV0 | 假 NTT |
| 530 | L2 AIV0 | 假点积 |
| 540 | L2 AIV0 | 假 INTT |
| 502 | L2 AIV0 | 已 SET(4) |
| 510 | L2 AIV1 | AIV1 入口 |
| 521 | L2 AIV1 | 假 NTT |
| 531 | L2 AIV1 | 假点积 |
| 541 | L2 AIV1 | 假 INTT |
| 512 | L2 AIV1 | 已 SET(4) |

**判读**：同进程默认 **3×** Host `100/101/105/110/111`；每轮设备侧先见 L1 `200→201→202→203`，再进 L2 `400/401/402` 与 AIV0 `500→520→530→540→502`、AIV1 `510→521→531→541→512`。  
缺任一段 = FAIL。未测 OMIT_SET4 / 双 Cube / GATE alone。
