# frozen-gates — Encrypt 分阶段 Gate 过渡路线（G0–G4 已关闭）

**前缀**：`frozen-<简述>/`（本探针内；与 `compute/frozen/` 内核路线冻结并列）

**治理规则**（2026-06-30 定稿）：

1. **G5** = 唯一**生产验收**路径（默认 `ENCRYPT_GATE=5`，`bash run.sh`）。
2. **G0–G4** = **过渡路线**：分段拼装、staging、Host 绕行；**每测通下一 Gate 即冻结上一 Gate**。
3. G5 双模式 PASS 后，**G0–G4 全部关闭**；禁止作为交付或新 feature 基线；仅历史回放 / 审计只读。

## Gate 阶梯与冻结链

| Gate | 测通后冻结 | 过渡路线特征 | 关闭日期 | 继任 |
|------|------------|--------------|----------|------|
| **G0** | —（起点） | marker 壳 only | 2026-06-30 | G1+ |
| **G1** | G2 测通 → **G0 冻结** | prep 分段 + 中间落盘 | 2026-06-30 | G5 内 prep launch |
| **G2** | G3 测通 → **G1 冻结** | + NTT(r̂)，仍 staging | 2026-06-30 | G5 内 ntt_r |
| **G3** | G4 测通 → **G2 冻结** | 线性层；旧四核 / 多 session / `t_hat.bin` staging | 2026-06-30 | G5 `at_r5` + device decode |
| **G4** | G5 测通 → **G3 冻结** | INTT + **Host 标量** noise/pack（SIM 507000 绕行）| 2026-06-30 | G5 `run_g5_sim_full` 全 device |
| **G5** | —（**当前活跃**） | 单 session；`input/` 仅 ek/m/coins；全 device 计算 | — | stable 晋级（待定） |

## 冻结产物目录

| 目录 | 内容 | 原 Gate | 关闭原因 |
|------|------|---------|----------|
| [`frozen-g4-host-scalar-tail/`](frozen-g4-host-scalar-tail/) | `f203_encrypt_g4_host_scalar.hpp` / `f203_encrypt_pack_host_scalar.hpp` | G4 SIM | G5 前 SIM 上 g4_noise/pack 507000 时的 Host 代算绕行；**违反 device 全算** |
| [`frozen-g4-split-kernels/`](frozen-g4-split-kernels/) | `g4_add_e1` / `g4_make_v` 拆分核 | G4 早期 | 已由 `f203_encrypt_g4_noise` 六参合并 |
| [`../compute/frozen/`](../compute/frozen/) | 旧 G3 四核（g3_linear/at_r/t_dot_r） | G3 | func_key≥5 / 多 session；继任 `at_r5` |

## 仍留在 `main_encrypt.cpp` 的过渡代码（G0–G4）

`ENCRYPT_GATE=0..4` 分支**未删**，供历史 log 回放；启动时打印 **`[WARN] 过渡路线 Gn 已冻结`**。  
**禁止**：新功能基于 gate&lt;5 扩展；文档/验收不得默认 gate&lt;5。

| 符号 | Gate | 说明 |
|------|------|------|
| `launch_marker` | G0 | marker 壳 |
| `run_prep_*` / gate&lt;2 早退 | G1 | 分段 prep |
| `run_ntt_r` / gate&lt;3 | G2 | 分段 NTT |
| `run_g3_device_sim_once` + `input/t_hat.bin` | G3 | 旧 SIM G3 编排（gate=3/4 非 G5） |
| `run_g4_tail_sim_once` | G4 | device INTT + **frozen host scalar** tail |

合法全链：**仅** `run_encrypt_g5_cpu_full` / `run_g5_sim_full`（`main_encrypt_g5_run.cpp`）。

## Agent 规则

- **可进入**本目录与 [`G3_SIM_AUDIT.md`](../G3_SIM_AUDIT.md) 读关闭原因
- **禁止**把 frozen-gates 内源码复制到活跃 kernel / G5 路径
- **禁止**把 G4 host scalar 重新接入默认 SIM 路径
- 验收命令：**不得**写 `ENCRYPT_GATE=0..4` 作为标准用法
