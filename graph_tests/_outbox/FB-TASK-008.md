# FEEDBACK TASK-008

## 结果摘要
- outcome: PASS（SIM 基线）
- one_liner: 默认 2-launch Decaps **SIM 仍绿**（FORCE+SIM_DIRECT；`chain_ntt`/`prep_ntt`/`l18+FO`；K max=0）；K=accept≠J(z‖c) → **support H4**；NPU K=131 转用户正交。

## 对图谱的影响
| node_id | effect | evidence_path |
|---------|--------|---------------|
| D-next-reverify-sim | **support** → 主控标 inactive | `/opt/cursor/artifacts/task008_decaps_sim_step0.log` |
| F-sim-reverify-pass | **新事实 candidate（主控已入库）** | 同上；wall≈284.5s |
| Q-sim-repro-now | **answered：是** | max_abs_diff=0 |
| F-sim-k-accept-not-jzc | **新事实** | `/opt/cursor/artifacts/task008_k_vs_jzc_diag.txt`（K vs J=249） |
| J-k131-reject-path | **weaken on SIM**（SIM 未误入拒绝）；NPU 仍待验 | 诊断 |
| J-sim-green-then-npu-sync | **support** | SIM 绿 + 历史 NPU 红 |
| D-sim-orthogonal-if-red | n/a（SIM 未红，未跑 A/B/C） | — |
| D-user-npu-abc | **建议下一刀** | qa§6 |

## 实际改动
- files: `graph_tests/_outbox/FB-TASK-008.md`（本文件）；主控另刷 decrypt yaml
- 未改：stable 源码；无 commit/push

## 命令与关键日志
```bash
KEM_DECAPS_FORCE_REBUILD=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# [kem-dec-d] launch 1 f203_decrypt_g4_chain_ntt
# [kem-dec-e] launch 1 f203_kem_dec_phase_e_prep_ntt
# [kem-dec-e] launch 2 f203_encrypt_l18_l19 (ySrc=null+FO)
# [verify] K.bin max_abs_diff=0 / PASS
# [wall_sec] 284.521
```

## SIM 墙钟
- sim_sec: 284.521（kernel 段；含 D+E 多 launch）

## 建议下一刀
- 用户 NPU：默认 + A/B/C；红样本算 K vs J(z‖c)（`dk_kem[3136:3168]` + `c.bin`）。
