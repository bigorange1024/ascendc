# TRACE.md — toy-e12-chain-k2-multipoly

**形态**：E11 壳扩 **k=2** — L1=SHAKE + CBD×2；L2=（NTT→basemul→INTT→Decompress_1(μ)→Compress→ByteEncode）×2 + SET(4)。  
**输出**：2×128B=256B；μ 32B 两 poly 共享。

## Host

| code | 含义 |
|------|------|
| 100 | 轮次开始 / L1 |
| 101 | L1 Sync |
| 105 | μ 就绪（ws[MU0]） |
| 110 | L2 |
| 111 | L2 Sync |

## L1

| code | 含义 |
|------|------|
| 200–203 | 同 E11 SHAKE |
| 222 | **k=2 CBD 完成** |

## L2 poly0（500 段）

| code | 含义 |
|------|------|
| 400–401 | AIC poly0 NTT/INTT |
| 500–562 | AIV0 真链（同 E11 542→546→552→562） |
| 503/513 | poly0 AIV0/1 段完成 |
| 510–553 | AIV1 对称 |

## L2 poly1（600 段）

| code | 含义 |
|------|------|
| 410–411 | AIC poly1 |
| 600–662 | AIV0（+60 偏移于 poly0） |
| 603/613 | poly1 段完成 |
| 610–653 | AIV1 |

## L2 收尾

| code | 含义 |
|------|------|
| 402 | AIC Wait(4) 成功 |
| 502/512 | AIV0/1 SET(4) |

**CrossCore**：NTT 1/2；INTT 5/6；壳 SET(4)=4。
