# FEEDBACK — T04-mu-embed-decompress1

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/bricks/RB-T04-mu-embed && bash run.sh -r cpu -v Ascend910B4 ; SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0 / 0
wall_min: 6
sync_audit: N/A（无 CrossCore）
sim: ok
notes:
- 新建 RB-T04：AIV-only；m[32]→μ[256]；ByteDecode₁ LSB + Decompress₁ (c·q+1)>>1。
- 固定 m=0xA5/0x5A 交替；golden 128 个 0 + 128 个 1665；CPU/SIM max=0。
- 未抄 encrypt/alg14/f203_mu_embed；未改 KB/DAG；未并行第二路 SIM。
- 建议日后可进 library/shared（仅建议，未迁）。
next_hint: 主控关闭 G-MU；下一刀本会话继续 T05 ByteDecode₁₂。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/cpu.log`](logs/cpu.log) | CPU 编译+verify 全量 |
| [`logs/sim.log`](logs/sim.log) | SIM_DIRECT 全量 |

## 实现 STATUS

[`../../../bricks/RB-T04-mu-embed/STATUS.md`](../../../bricks/RB-T04-mu-embed/STATUS.md)

## 主控批注

（待主控）
- 2026-09-08 主控 NPU 上板：**PASS**（RB-T04）；主控连续占卡中。
