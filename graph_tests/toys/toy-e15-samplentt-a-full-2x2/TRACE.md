# TRACE.md — toy-e15-samplentt-a-full-2x2

**形态**：E14 Encrypt 粘合 + **完整 2×2 Â 真 SampleNTT**（独立 launch phase）。  
**c 布局**：同 E13/E14 — `c1[0:256]=u0∥u1`；`c2[256:384]=v`。

## Host（Encrypt 三段 launch）

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
| 300 | 设备 SampleNTT 段开始 |
| 302 | (0,0) → G0 完成 |
| 303 | (0,1) → G1 完成 |
| 304 | (1,0) → G2 完成 |
| 305 | (1,1) → G3 完成 |

**范围**：完整 k×k=2×2 四元；v 路 basemul 读 G2=(1,0)；G3 采样完整性（L2 链未直接消费）。

## L2 u/v 真链（同 E13/E14 500/600/700 段）

CrossCore：NTT 1/2；INTT 5/6；SampleNTT 就绪 7；SET(4)=4。

**注**：SIM 下 v 路 746/747、AIV 502/512 偶发丢失；验收以 SampleNTT 300/302/303/304/305 + 402 + golden 为准。
