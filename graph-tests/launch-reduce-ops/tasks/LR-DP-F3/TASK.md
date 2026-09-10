# LR-DP-F3 — Decaps 3→2（Reenc prep 融进 enc MIX）

## 目标

新建 `enc_related/RB-T30-decaps-2launch`（**禁止改** T28/T29/D08/D09）：

Host：**2 launch** = L1 Decrypt 全链融合 + L2 Re-Encrypt 全链融合 → `K`≡liboqs；NPU×30。

## 方案要点

- L1：`t30_dec_fused_custom` — prep→SyncAll→NTT+dot→INTT+extract
- L2：`t30_enc_fused_custom` — prep(G/CBD/Â…)→SyncAll→NTT 路径（flag 1/3+GATE4）→pack c'
- Host FO：c'≡c ? K' : J(z‖c)
- 独立 basename / 重写实现；禁抄既有核

## runner

`main_npu` only。
