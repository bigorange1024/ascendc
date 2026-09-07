# TRACE.md — toy-e14-glue-plus-samplentt

**形态**：E13 Encrypt 粘合 + **L2 设备 SampleNTT(Â)** 替换 u 路 stub ĝ。  
**c 布局**：同 E13 — `c1[0:256]=u0∥u1`；`c2[256:384]=v`。

## Host（Encrypt 两段角色）

| code | 含义 |
|------|------|
| 100 | 轮次开始 / **L1 采样** launch |
| 101 | L1 Sync |
| 102 | L1 采样产物 D2H |
| 105 | μ 就绪（v 路；L2 前） |
| 110 | **L2 代数+压码** launch |
| 111 | L2 Sync |

## L2 SampleNTT launch（phase=1；AIV subBlock0）

| code | 含义 |
|------|------|
| 104 | Host 启动 SampleNTT launch |
| 106 | Host SampleNTT Sync |
| 300/302/303 | 设备 (0,0)/(0,1) â 完成 |

**范围说明**：本刀仅 u 路 **2/4** 矩阵元（第一行 A[0,0]/A[0,1]）；v 路 G2 仍 Host stub。完整 2×2 留后续 TASK。

## L2 u/v 真链（同 E13 500/600/700 段）

CrossCore：NTT 1/2；INTT 5/6；SampleNTT 就绪 7；SET(4)=4。

**注**：SIM 下 v 路 746/747、AIV 502/512 偶发丢失；验收以 SampleNTT 300/302/303 + 402 + golden 为准。
