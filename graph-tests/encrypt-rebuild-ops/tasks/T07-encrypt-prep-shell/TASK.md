# T07 — Encrypt prep 壳（G4）

| 字段 | 值 |
|------|-----|
| 状态 | **blocked_dep**（建议 T01 PASS；硬依赖无算法前驱） |
| DAG | `D-EXP-T07` → G4 |
| 代码目录 | `graph-tests/enc_related/RB-T07-prep-shell/` |
| 运营目录 | `…/tasks/T07-encrypt-prep-shell/` |
| 墙钟 | ≤ 50 min |

继承 COMMON。

## 目标

新 I/O 壳：从 **ek 尾取 ρ**；**coins → (y, e₁, e₂)**（Alg.8 CBD η=2 + PRF/SHAKE）。  
可 Host 编排 + 调用 shared / 复用活跃 Alg.7/8 **积木接口**（链接或进程内），输出 bin 供后刀。  
若上设备：仅 prep 相关 launch，**禁**抄 KeyGen/Encrypt prep 源文件。

## 必读

- inventory G4 · Alg.7/8 notes  
- `pass-fix-f203-alg8-cbd-eta2-k4`、`alg7-sample-ntt`、`alg13-lines8-15` 的 **I/O 契约**（勿抄码）

## 验收

- 固定 ek/coins → y/e₁/e₂（及 ρ）与 golden 一致  
- CPU；设备可选  
- FEEDBACK：与 KeyGen SEED 路径差异说明（一行）

## 依赖

建议 T01 PASS（工程习惯）；QUEUE 标 blocked_dep 至 T01。
