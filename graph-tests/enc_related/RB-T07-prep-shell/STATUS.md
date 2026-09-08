# STATUS — RB-T07-prep-shell

| 字段 | 值 |
|------|-----|
| 刀 | T07 · D-EXP-T07 / G4 |
| 状态 | **PASS**（Host CPU；无设备核） |
| 日期 | 2026-09-08 |
| 墙钟 | ~3 min |

## 目标达成

1. 新 I/O 壳：`ρ←ek` 尾 32B；`coins→(y,e₁,e₂)`（Alg.8 CBD η=2 + SHAKE256 PRF）
2. Host 编排调用 shared `golden_se_sampling.sample_poly_cbd2`；**未**抄 KeyGen/Encrypt prep 源文件
3. 固定 ek/coins → 输出 bin 与 golden 逐字节一致
4. 与 KeyGen SEED 差异：PRF 种子直接是 coins（不用 SEED_D→G→σ）

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS |
| 设备 SIM/NPU | 本刀无核；`run.sh -r sim` 显式 SKIP |
| sync_audit | N/A（无 AscendC / CrossCore） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T07-encrypt-prep-shell/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T07-encrypt-prep-shell/FEEDBACK.md)
