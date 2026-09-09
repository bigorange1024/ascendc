# Decrypt/Decaps 重建 · 加压证据矩阵

> 刷新：2026-09-09 17:31 · 中间用例默认 ×30；往返曾 ≈100 + smoke×10

| 用例 | NPU×1 | NPU×30 | 备注 |
|------|-------|--------|------|
| RB-D01 prep | PASS（回填） | **ok=30** @17:15 | |
| RB-D02 ntt-dot | PASS | **ok=30** @17:17 | |
| RB-D03 intt-m | PASS | **ok=30** @17:28 | 曾假衔尾，强制重跑 |
| RB-D04 decrypt-full | PASS | **ok=30** | |
| RB-D04-decaps-G (K01) | PASS | **ok=30** | |
| RB-D05 reenc (K02) | PASS | **ok=30** | |
| RB-D06 fo (K03) | PASS | **ok=30** | 合法+拒绝 |
| RB-D07 RT (K04) | PASS | ×30 + 补≈100 + smoke×10 | |

门禁：Q-DEC / Q-DECAPS / Q-RT-HANG **closed**。
