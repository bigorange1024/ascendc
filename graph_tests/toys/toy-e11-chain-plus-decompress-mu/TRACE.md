# TRACE.md — toy-e11-chain-plus-decompress-mu

**形态**：E10 壳 — L1=**真** SHAKE256 + **真** CBD(η=2)；L2=**真** NTT + **真** basemul + **真** INTT + **真** Decompress_1(μ) + **真** Compress_d(d=4) + **真** ByteEncode_d(128B) + SET(4)。  
**语义**：Decompress_1=消息嵌入（μ 32B→256 系数 {0,1665} mod-q 加 INTT）；Compress/ByteEncode d=4。

## Host

| code | 含义 |
|------|------|
| 100 | 轮次开始 / 将 launch L1 |
| 101 | L1 Sync 完成 |
| 105 | Host μ 就绪（ws[MU0] 已 H2D） |
| 110 | 将 launch L2 |
| 111 | L2 Sync 完成 |

## L1（真 SHAKE → 真 CBD）

同 E10：`200→210→211→212→220→221→203`。

## L2

| code | 核 | 含义 |
|------|----|------|
| 400 | AIC | L2 AIC 入口 |
| 401 | AIC | 将 Wait(4) |
| 402 | AIC | Wait(4) 成功 |
| 500 | L2 AIV0 | 入口 |
| 520 | L2 AIV0 | 真 NTT 开始 |
| 530 | L2 AIV0 | 真 basemul 开始 |
| 532 | L2 AIV0 | 真 basemul 完成 |
| 540 | L2 AIV0 | 真 INTT 开始 |
| 542 | L2 AIV0 | 真 INTT 完成 |
| 544 | L2 AIV0 | **真 Decompress_1(μ) 开始** |
| 546 | L2 AIV0 | 真 Decompress_1(μ) 完成 |
| 550 | L2 AIV0 | 真 Compress_d 开始 |
| 552 | L2 AIV0 | 真 Compress 完成 |
| 560 | L2 AIV0 | 真 ByteEncode_d 开始 |
| 562 | L2 AIV0 | 真 ByteEncode 完成 |
| 502 | L2 AIV0 | SET(4) 后出口 |
| 510 | L2 AIV1 | 入口 |
| 521 | L2 AIV1 | 真 NTT 开始 |
| 531 | L2 AIV1 | 真 basemul 开始 |
| 533 | L2 AIV1 | 真 basemul 完成 |
| 541 | L2 AIV1 | 真 INTT 开始 |
| 543 | L2 AIV1 | 真 INTT 完成 |
| 545 | L2 AIV1 | 真 Decompress_1(μ) 开始 |
| 547 | L2 AIV1 | 真 Decompress_1(μ) 完成 |
| 551 | L2 AIV1 | 真 Compress 开始 |
| 553 | L2 AIV1 | 真 Compress 完成 |
| 512 | L2 AIV1 | SET(4) 后出口 |

**判读**：每轮 L2 须 `542→544→546→550→552→560→562`（AIV0）及对称 543→545→547→551→553（AIV1）。

**CrossCore**：NTT flag 1/2；INTT flag 5/6；壳层 SET(4)=flag 4。
