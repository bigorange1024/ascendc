# Decrypt scalar / 握手优化 · QUEUE

> 权威计划：[`PLAN.md`](PLAN.md)。状态只在本表刷新。  
> **推荐上板序（W1 已排）**：**H2 → H3 → H1 → H4**（见 `tasks/DS-W1/HOTSPOTS.md`）。

| # | ID | 假说 | 目录 | 推荐序 | 状态 | 说明 |
|---|-----|------|------|--------|------|------|
| 0 | DS-W0 | H0 基线复测 | `dec_related/RB-D09-decrypt-1launch` | 0 | **完成** | ×30 绿；Σ≈454–456 µs；v0 scalar≈96.9%；见 `tasks/DS-W0/FEEDBACK.md` |
| 1 | DS-W1 | 静态热点清单 | `tasks/DS-W1/HOTSPOTS.md` | 1 | **完成（离线）** | 序：H2→H3→H1→H4 |
| 2 | DS-H2 | Prep/解码向量化 | `RB-D10b-prep-vec` | **2** | **完成 · 强成功** | Σ **334.88µs** ≈0.74×W0；×30 绿；见 `tasks/DS-H2/FEEDBACK.md` |
| 3 | DS-H3 | NTT/INTT 向量化 | `RB-D10c-ntt-vec`（待建） | **3** | **下一刀** | 蝶形 for 环；scalar% 仍 ~95.6% |
| 4 | DS-H1 | 握手屏障减薄 | `RB-D10a-handshake-thin`（待建） | **4** | 阻塞于 H2/H3 | 补刀 |
| 5 | DS-H4 | 双 AIV 分担 | `RB-D10d-dual-aiv`（待建） | **5** | 靠后 | 仅收益不足时 |
| 6 | DS-CLOSE | 收口 | qa / HANDOFF / 图谱 | 末 | 阻塞于采纳或全证伪 | 写总表+纪要 |

## 门禁速查

- 正确：NPU×30 · m≡liboqs  
- 形态：launch=1 · flag∈{1,3} · sync_audit  
- 性能：Σ µs ≤0.90× 基线（有条件成功） / ≤0.80×（强成功）
