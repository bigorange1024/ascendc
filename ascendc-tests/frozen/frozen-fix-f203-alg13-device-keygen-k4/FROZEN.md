# FROZEN — pass-fix-f203-alg13-device-keygen-k4（2026-06-29 关闭）

## 原角色

FIPS 203 Alg.13 ML-KEM-768（k=4）**PKE KeyGen** 全链探针：2 launch（prep + mmad），生产 I/O。

## 关闭原因

- prep 段 **block0 串行两片 Â**（`BuildAHat16ShardWithUb` 0+1 在 block0 内顺序执行）为 SIM 正确性 workaround，**非终态最优路径**。
- SIM 910B4 Total tick **≈886801**（prep ≈806k），相对双 AIV 并行 fork 慢约 **39%**。
- 2026-06-29 已在 fork 探针证 prep **双 AIV 并行 Â** + liboqs KAT（CPU×10 + SIM×1）PASS。

## 合法继任（禁止从本目录抄码）

| 路径 | 说明 |
|------|------|
| [`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/) | 原 `fix-f203-alg13-device-keygen-k4-dual-aiv` 晋级；SIM **≈542339** |
| [`exp-fips203-mlkem-pke-keygen-k4`](../../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/) | 自包含交付示例，与 pass 终态对齐 |

## 讨论

[`qa/2026-06/2026-06-29-KeyGen双AIV并行fork探针.md`](../../qa/2026-06/2026-06-29-KeyGen双AIV并行fork探针.md)

## 定稿

[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)
