# Decrypt scalar / 握手优化 · QUEUE

> 权威计划：[`PLAN.md`](PLAN.md)。状态只在本表刷新。  
> **推荐上板序（W1 已排）**：**H2 → H3 → H1 → H4**（见 `tasks/DS-W1/HOTSPOTS.md`）。

| # | ID | 假说 | 目录 | 推荐序 | 状态 | 说明 |
|---|-----|------|------|--------|------|------|
| 0 | DS-W0 | H0 基线复测 | `dec_related/RB-D09-decrypt-1launch` | 0 | **待上机** | 钉 µs / scalar%；偏差&gt;5% 先查环境 |
| 1 | DS-W1 | 静态热点清单 | `tasks/DS-W1/HOTSPOTS.md` | 1 | **完成（离线）** | 序：H2→H3→H1→H4 |
| 2 | DS-H2 | Prep/解码向量化 | `RB-D10b-prep-vec`（待建） | **2** | 阻塞于 W0 | **下一实现刀** |
| 3 | DS-H3 | NTT/INTT 向量化 | `RB-D10c-ntt-vec`（待建） | **3** | 阻塞于 H2 判决 | 蝶形 for 环 |
| 4 | DS-H1 | 握手屏障减薄 | `RB-D10a-handshake-thin`（待建） | **4** | 阻塞于 H2/H3 | 补刀 |
| 5 | DS-H4 | 双 AIV 分担 | `RB-D10d-dual-aiv`（待建） | **5** | 靠后 | 仅收益不足时 |
| 6 | DS-CLOSE | 收口 | qa / HANDOFF / 图谱 | 末 | 阻塞于采纳或全证伪 | 写总表+纪要 |

## 门禁速查

- 正确：NPU×30 · m≡liboqs  
- 形态：launch=1 · flag∈{1,3} · sync_audit  
- 性能：Σ µs ≤0.90× 基线（有条件成功） / ≤0.80×（强成功）
