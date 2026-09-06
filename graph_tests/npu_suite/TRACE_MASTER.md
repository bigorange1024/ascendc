# TRACE_MASTER — 套件级三位数字总表

对齐 [`docs/notes/Encrypt-实机无卡死-知识库.md`](../../docs/notes/Encrypt-实机无卡死-知识库.md) §6。  
各档详细号表见原 toy 的 `TRACE.md`（只读引用，号**不复用改义**）。

**回报约定**：前缀 case  id — `C0:` / `C1:` / `C2:` + 数字序列（例 `C1: 100 101 102 105 110`）。

---

## 段含义（全套件通用）

| 段 | 谁 | 含义 |
|----|----|------|
| 1xx | Host | launch 前后、Sync 返回、D2H、μ 就绪 |
| 2xx | L1 AIC/AIV | SHAKE / CBD 采样 |
| 3xx | L2 AIV | SampleNTT phase（**仅 C2**） |
| 4xx | L2 AIC | NTT/INTT Cube、Wait(4)/GATE |
| 5xx–7xx | L2 AIV | u/v 真链、Compress、ByteEncode、SET(4) |
| 9xx | 任一侧 | 失败/超时哨兵 |

---

## 骨架号（C0/C1/C2 共用）

| 号 | 位置 | 出现 ⇒ | 未出现 ⇒ |
|----|------|--------|----------|
| 100 | Host 将 launch L1 | 进入 L1 | Host 未发 L1 |
| 101 | Host L1 Sync 回 | L1 完成 | 卡在 L1 |
| 110 | Host 将 launch L2 | 进入 L2 | 未发 L2 |
| 111 | Host L2 Sync 回 | **整段成功关键点** | **卡在 L2（历史主挂点）** |
| 400 | L2 AIC 入口 | AIC 已进 L2 | AIC 未进 |
| 401 | L2 AIC 将 Wait(4) | 到达 Wait | 未到 Wait |
| 402 | L2 AIC Wait(4) 后 | **SET 配对成功** | 死等 SET |
| 500/510 | L2 AIV0/1 入口 | 该 AIV 已进 | 该 AIV 未进 |
| 502/512 | 已 SET(4) | 旗语已发 | 未发 SET |
| 900 | 哨兵 | 主动失败 | — |

**C0 专用**：仅上表 + 每轮 `400/401/402` 与 `500/502`、`510/512`（无 2xx/3xx/5xx 业务链）。

---

## C1 增量（形态粘合 · 详表 `toy-e13/TRACE.md`）

| 号段 | 含义 |
|------|------|
| 102 | L1 采样产物 D2H |
| 105 | μ 就绪（v 路；L2 前） |
| 200–223 | L1 SHAKE + u/v CBD |
| 400–411 | AIC u0/u1 NTT/INTT |
| 420–421 | AIC v NTT/INTT |
| 500–662 | AIV u0/u1 真链（Compress+ByteEncode） |
| 700–762 | AIV v 真链 + Decompress(μ) 744/745 |
| 402 | AIC 三 poly 完成 |

**判读要点**：Host 有 `110` 无 `111` 且设备无 `402` → 对齐 Encaps 历史 l18 挂点。

---

## C2 增量（Â 2×2 SampleNTT · 详表 `toy-e15/TRACE.md`）

在 C1 基础上增加 **独立 SampleNTT launch**：

| 号 | 含义 |
|----|------|
| 104 | Host 启动 SampleNTT launch |
| 106 | Host SampleNTT Sync |
| 300 | 设备 SampleNTT 段开始 |
| 302 | (0,0)→G0 完成 |
| 303 | (0,1)→G1 完成 |
| 304 | (1,0)→G2 完成 |
| 305 | (1,1)→G3 完成 |

**期望序（每轮）**：`100 101 102` → `104 106` → `105` → `110` →（设备 `300 302 303 304 305` + u/v 链）→ `111`。

---

## 跨档对比（挂因阶梯）

| 对比 | 若 C0 绿 C1 挂 | 若 C1 绿 C2 挂 |
|------|----------------|----------------|
| 缩小范围 | 握手外：采样/真链/压码 | SampleNTT phase 或 2×2 编排 |
| 重点查 | `105`→`110` 间 Host；L2 无 `4xx` | `104/106` 与 `300–305`；G2/G3 后 L2 链 |
| 历史对齐 | Encrypt L2 编排 | Alg.7 独立 launch + L2 汇合 |

---

## 号段冲突策略

- 三位号**全局语义一致**（同号同义）；C2 是 C1 的超集。  
- 设备 plog 与 Host tee **可能乱序**；回报以**时间序**为准。  
- SIM 偶发丢 `502/512` 或 v 路 746/747 — **NPU 取证仍以最后稳定号为准**，不以 SIM 丢号为据。
