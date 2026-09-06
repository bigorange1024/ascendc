# ORIGIN-glue.md — E13 Encrypt 形态粘合

## 来源

- **壳**：复制 `toy-e12-chain-k2-multipoly/`（未改 E12）
- **粘合语义**：呈现 ML-KEM Encrypt 两段 Host 角色 + c 形输出；**非**抄 `examples/` Encrypt/Encaps 源码

## Encrypt 形态映射（玩具 k=2）

| Encrypt 概念 | 本 toy 实现 |
|--------------|-------------|
| **L1 采样** | Launch1：`SHAKE` + `CBD(u×2→src)` + `CBD(v→ws[E0])` |
| Host 间隙 | D2H 采样产物；H2D `μ`（32B） |
| **L2 代数+压码** | Launch2：u 路×2 真链（NTT→basemul→INTT→Compress→ByteEncode，**无 μ**） |
| | v 路×1 真链（同上 + **Decompress_1(μ)**） |
| **c 输出** | `out[0:256]=c1`；`out[256:384]=c2` |
| 公钥材料 | `g.bin` 768 int32 = ĝ_u0∥ĝ_u1∥ĝ_v（确定性 stub）；`M4/Minv4` 同 E12 toy 矩阵 |

## 与 E12 差异

| 项 | E12 | E13 |
|----|-----|-----|
| L1 | CBD×2 | CBD×2 **+ v 路 e2** |
| L2 Decompress | 两 u poly **均** +μ | **仅 v 路** +μ |
| 输出 | 256B（2×encode，语义对称） | **384B c1∥c2**（u/v 分路） |
| prf | 256B | **384B** |
| g | 512 int32 | **768 int32**（+ĝ_v） |
| Host TRACE | 100/105/110 | **+102**（采样 D2H） |

## 未采用

- 抄 `examples/incubating/.../encrypt` 或 Encaps host 编排
- 5 路 CBD（r/e1/e2 全量 ML-KEM）；本刀仅 u×2 + v×1 最小 c 形
- 并行 u poly 占双 AIV
- polyvec 矩阵乘（仍用单 poly basemul + stub ĝ）

## 公钥 stub 说明

- `ĝ_p[i] = (13·i + 7 + p·997) mod q`，p∈{0,1,2}
- `M4/Minv4`：E04 同款 256×256 toy 矩阵；**非**真实 A^T·r 语义，仅验积木链接 + golden 自洽
- 权威 liboqs 交叉：**未跑**（本 TASK 范围外）
