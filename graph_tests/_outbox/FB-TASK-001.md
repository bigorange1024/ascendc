# FEEDBACK TASK-001

## 结果摘要
- outcome: PASS
- one_liner: 新建 `fix-encrypt-skel-mix-chain-toy`：1 MIX launch + CrossCore 1/3 双轮 + stub 链；SIM magic 绿，kernel ~3.6s。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| Q-toy-repro | support | 最小骨架 SIM 正常结束且快（非挂死）：`STATUS.md`；log `[wall_sec] 3.640` / magic OK → 证伪「任何 MIX+1/3 必挂」的极端读法，开放「何种骨架复现挂」仍待加重逼近 |
| J-common-mix-flag13 | weaken | 同构 MIX + flag 1/3 双轮握手在轻量 stub 下 **SIM 不挂**（tick 19663）；说明挂点需额外因子（GATE/真哈希/大 tiling/多轮 Cube 等），1/3  alone 非充分条件 |
| D-near-toy | support | 轻量骨架已落地且 SIM 通 |
| D-next-build-toy | support | 探针目录可用作后续逼近假说的基线 |
| D-verify-sim-for-npu | support | 本单仅 SIM 验收，符合门禁 |
| D-forbid-syncall-while-wait | n/a | 实现中未使用 SyncAll |
| D-softsync-follow-decrypt | n/a | 未自造 SoftSync |
| D-reject-correctness-antipattern | support | 单 launch、无碎写 GM、stub 非真哈希 |
| D-solo-subagent-timebox | support | 单次 SIM 一次通过，无返工 |
| D-no-autonomous-push | support | 未 commit/push |

> 说明：本实验 **weaken**「1/3 为充分挂因」，**未 refute**「全量 Encrypt 路径上 1/3 为公因子」；后续应在本骨架上增量逼近（加 GATE / 增大 MMAD / 真 LUT）以继续检验 J。

## 实际改动
- files:
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/`（整树新建：kernel/host/CMake/run.sh/scripts/STATUS）
  - `ascendc-tests/INDEX.md`（一行）
  - `graph_tests/INDEX.md`（用例表/工单状态）
  - `graph_tests/_outbox/FB-TASK-001.md`（本文件）
- 未改（说明）：`docs/rg-kem-encrypt-hang.yaml`（write_graph: no）；未动 stable/frozen/rules/skills；API 均复用查阅索引已有记录，未改索引文件（白名单外）

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 退出码 0
# 关键尾部：
[INFO] Model Start Time: 2026-09-03 10:03:47
[INFO] Model Stop Time: 2026-09-03 10:03:50
Model RUN TIME: 3191.53 ms
[INFO] Total tick: 19663
[INFO] Model stopped successfully.
[wall_sec] 3.640
[kernel-run-timeout] wall_sec=3.640 budget=180s rc=0 bin=ascendc_kernels_bbit
[SUCCESS] magic OK (64 B, prefix=b'SKELENC1')
[SUCCESS] fix-encrypt-skel-mix-chain-toy (sim)
ELAPSED_SEC=10.34   # 含 cmake 编译；纯 kernel 墙钟见 wall_sec
```
完整日志：`/opt/cursor/artifacts/task001_skel_toy_sim.log`（或 `/tmp/skel-toy-sim.log`）

## SIM 墙钟（必填若跑了 sim）
- sim_sec: 3.640（kernel `wall_sec`；含编译整脚本约 10.34s）
- vs_full_encaps: faster

## 意外发现（新事实候选，勿写成长叙事）
- 轻量 16×32×32 双轮 flag 1/3 在 SIM 上稳定绿；dumps 落在 `sim_log/`，用例根无 stray `core*.dump`。
- AIV1 仅参与 SET(1)/SET 对称握手、不写 S0，与 Encrypt「双 AIV 均 SET」同构且无 SyncAll。

## 建议下一刀（可选，主控可不采纳）
- 在本探针上增量：① 插入 GATE 4/8 空转；② 把 MMAD 放大或连续四路；③ 观察是否逼近挂死，以继续锤 J-common-mix-flag13。
