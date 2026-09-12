# DS-W0 FEEDBACK（完成）

| 项 | 值 |
|----|-----|
| 日期 | 2026-09-11 |
| 节点 | `cannlab-npu-2` / `100.70.72.104:2222`（MagicDNS；须 SOCKS `127.0.0.1:1055`） |
| 仓库 tip | `5e7f4f5`（sticky） |
| 设备 | npu-smi NPU **1** · 910B3 · Health OK；`ASCEND_DEVICE_ID=0` |
| Freq | Rated **1800**；空闲 `npu-smi` curFreq 曾见 **800**（k8s 内 `set frequency` 被拒）；**msopprof 核内 Current Freq=1800** |
| 依赖 | `ntt_onnx` 表头已齐；`liboqs` + `liboqs_pke_ref` 已板上编过（job `ds-w0c`） |
| 编译 | **PASS** |
| NPU 单轮 | **PASS** · oracle=liboqs · launch=1 |
| NPU×5 smoke | **5/5**（`ds-w0c`） |
| NPU×30 | **30/30**（job `ds-w0d_20260911_111856`，SEED_D=3037…） |
| Σ Task Duration | **454.12–455.52 µs**（msopprof；三轮 f1/f2/f3） |
| vector0 scalar% | **≈96.9%**（`aiv_scalar_ratio` ≈ 0.9685–0.9696） |
| 相对战役基线 | 基线 Σ≈455–456 / scalar≈96.8%；偏差 **&lt;1%** → **环境可信，可开 H2** |
| 判决 | **W0 通过（正确性+性能钉桩）**；容器锁频命令不可用，但 profiling 口径已是 1800/1800 |
| 日志 | `/mnt/workspace/launch-reduce-logs/d09-w0d-20260911-191912/` · msopprof `/home/developer/opprof_priv/d09_w0e_20260911-192919/` |

## 环境备注

1. `which_npu` 可能误选离线 `cannlab-npu`；上机用 `SSH_HOST_FORCE=100.70.72.104` 或自动 pick 的 `cannlab-npu-2`。  
2. sshd 须监听 Tailscale IP:2222（仅 `127.0.0.1` 时 Agent 进不去）。  
3. msopprof 输出目录须 **0700**（组/其他人可写会被拒）。  
4. 跑 binary 前：`LD_LIBRARY_PATH=<case>/out/lib:$LD_LIBRARY_PATH`。

## 下一刀

按 QUEUE：**DS-H2** — 新树 `RB-D10b-prep-vec`（Prep/解码向量化）；不改 D09 源码。
