# FEEDBACK — T07-encrypt-prep-shell

> Subagent 回填；主控只在文末「主控批注」追加。

```
ID: PASS
cmd: cd graph-tests/enc_related/RB-T07-prep-shell && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: 3
sync_audit: N/A（无 AscendC / CrossCore）
sim: skip（本刀无设备核；TASK 设备可选）
notes:
- Host 壳：ρ←ek[1568] 尾 32B；coins→y[4×256]/e1[4×256]/e2[256]（SHAKE256 PRF + CBD2）。
- 复用 shared golden_se_sampling.sample_poly_cbd2；未抄 KeyGen/Encrypt prep 源文件；未改 KB/DAG。
- 与 KeyGen SEED 路径差异：PRF 种子直接是 coins，不用 SEED_D→G→σ。
- 输出 bin 供后刀：rho/y/e1/e2/y_e1_e2；见 LAYOUT.md。
next_hint: 主控可回收 T03+T07；后刀可接 SampleNTT(ρ) 或 NTT(y)。
```

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/cpu.log`](logs/cpu.log) | Host CPU 全量 |

## 实现 STATUS

[`../../../enc_related/RB-T07-prep-shell/STATUS.md`](../../../enc_related/RB-T07-prep-shell/STATUS.md)

## 主控批注

- 回收 [催T03勿停](6ba4e998-63af-4256-b73c-f73ab8a5c6bb)：Host CPU PASS；sim SKIP（无设备核）— **不要求 NPU 双绿**。
- 产出 bin 供后刀 SampleNTT(ρ)/NTT(y)；已随树 rsync 至远端 `enc_related/RB-T07-prep-shell`。

（待主控）
