# NPU / 实验状态（KeyGen · 人话）

> 刷新：2026-09-09（Local 交接 Cloud；待开机）

## 战役目标

| 目标 | 结果 |
|------|------|
| PKE KeyGen ≡ liboqs_pke，且 NPU×30 不挂 | CPU 绿；**NPU 未跑** |
| KEM KeyGen ≡ liboqs_kem，且 NPU×30 不挂 | CPU 绿（K02）；**NPU 未跑** |

云机：交接时 **未要求保持开机**。下一任 Cloud：用户开机后按 HANDOFF P0 上板。

## 各实验

| 代号 | 测什么 | 对拍 | 结果 |
|------|--------|------|------|
| P01–P03 | prep / NTT / dot+encode 砖 | host/FIPS oracle | PASS_CPU |
| P04 | PKE 三 launch 全链 | liboqs_pke | PASS_CPU；wait_npu |
| K01 | KEM tail（H(ek)/z/dk） | host/K01 契约 | PASS_CPU；wait_npu |
| K02 | KEM 四 launch 全链 | liboqs_kem | PASS_CPU；wait_npu |

细表：[`MATRIX.md`](MATRIX.md)。约束：[`COMMON.md`](COMMON.md)。交接：[`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md)。
