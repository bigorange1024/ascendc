# FEEDBACK · C1 cann-ntt Encrypt 2-launch

**状态**：PASS  
**目录**：`graph-tests/enc_cann_ntt/EN15-encrypt-2launch/`  
**日期**：2026-09-12

## 结果

| 项 | 值 |
|----|-----|
| Host `ACLRT_LAUNCH_KERNEL` | **2**（`enc_prep_l1` + `enc_compute_l2`；主程序审计 `host_launch_count=2`） |
| CPU | exit 0；c≡liboqs **max=0**；wall≈2.2s |
| SIM | exit 0；c≡liboqs **max=0**；tick **898767**；wall≈153.7s |
| 禁令 | 未抄 examples/frozen/RB-T*/ER0* 核；未改 EN13 原地假称 2-launch；未跑 npu |

## 命令

```bash
cd graph-tests/enc_cann_ntt/EN15-encrypt-2launch
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

日志：`/opt/cursor/artifacts/low-launch-sim/en15-*.log`

## 实现要点

- L1：融 SampleNTT+CBD；Host mid 仅 Âᵀ + 喂 L2 输入。
- L2：MIX 串 NTT→matvec→dot→INTT×2→加噪→pack；CrossCore 仅 flag 1/2/3；阶段间 SyncAll（Wait 环外）。
- 相对 EN13：8→2 launch；中间段 dump soft-skip（硬门禁只验 c）。

## 下一步

C2 Encaps（EP06）同构 + Host FO；本刀无阻塞。
