# TRACE.md — toy-e13-encrypt-shaped-glue

**形态**：E12 积木 + **Encrypt 形态粘合** — L1=采样 / L2=代数+压码 → **c=c1||c2(384B)**。  
**c 布局**：`c1[0:256]=u0∥u1`（无 μ）；`c2[256:384]=v`（含 Decompress_1(μ)）。

## Host（Encrypt 两段角色）

| code | 含义 |
|------|------|
| 100 | 轮次开始 / **L1 采样** launch |
| 101 | L1 Sync |
| 102 | L1 采样产物 D2H（SHAKE + u 路 CBD） |
| 105 | μ 就绪（v 路消息；L2 前） |
| 110 | **L2 代数+压码** launch |
| 111 | L2 Sync |

## L1 采样

| code | 含义 |
|------|------|
| 200–203 | SHAKE（同 E11/E12） |
| 222 | u 路 CBD×2 完成 |
| 223 | **v 路 e2 CBD 完成** |

## L2 u 路 c1（500/600 段；**无** μ 嵌入 TRACE 544–547）

| code | 含义 |
|------|------|
| 400–401 / 410–411 | AIC u0/u1 NTT/INTT |
| 500–562 / 600–662 | AIV 真链（Compress+ByteEncode） |
| 503/603 | u poly 段完成 |

## L2 v 路 c2（700 段；含 μ）

| code | 含义 |
|------|------|
| 420–421 | AIC v NTT/INTT |
| 700–762 | AIV v 真链 + Decompress(μ) 前导 744/745 |
| 703/713 | v 段完成 |

## L2 收尾

| code | 含义 |
|------|------|
| 402 | AIC 三 poly 完成 / Wait(4) 前 |
| 502/512 | AIV SET(4)（SIM 偶发丢失；以 402 + golden 为准） |

**注**：SIM 下 v 路 Decompress 后导 TRACE（746/747）偶发丢失；验收以 744/745 + golden 为准。

**CrossCore**：NTT 1/2；INTT 5/6；SET(4)=4。
