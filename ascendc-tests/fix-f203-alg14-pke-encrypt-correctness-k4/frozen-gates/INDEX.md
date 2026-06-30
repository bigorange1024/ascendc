# frozen-gates — Encrypt Gate 过渡路线索引

**活跃**：**G5**（默认 `ENCRYPT_GATE=5`）— 见 [`../STATUS.md`](../STATUS.md)

**已关闭（G0–G4）**：见 [`FROZEN.md`](FROZEN.md)

| 子目录 | Gate | 说明 |
|--------|------|------|
| [`frozen-g4-host-scalar-tail/`](frozen-g4-host-scalar-tail/) | G4 | SIM Host 噪声 + pack 绕行 |
| [`frozen-g4-split-kernels/`](frozen-g4-split-kernels/) | G4 早期 | 拆分噪声核（未参编） |
| [`../compute/frozen/`](../compute/frozen/) | G3 | 旧四核 linear/at_r/t_dot_r |
