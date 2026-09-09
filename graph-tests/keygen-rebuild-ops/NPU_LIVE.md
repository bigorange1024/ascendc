# NPU / 实验状态（KeyGen · 人话）

> 刷新：2026-09-09（Cloud 上板收口）

## 战役目标

| 目标 | 结果 |
|------|------|
| PKE KeyGen ≡ liboqs_pke，且 NPU×30 不挂 | **达成**（RB-K04 ×30 pass=30） |
| KEM KeyGen ≡ liboqs_kem，且 NPU×30 不挂 | **达成**（RB-K05/K06 ×30 pass=30） |

云机：`cannlab-npu`（100.103.190.72）；工作树 `/mnt/workspace/ascendc-keygen` @ `dc44067` + tail 字面量修复。  
作业结束后已停 Cursor keepalive；卡空闲交平台/看门狗。

## 各实验

| 代号 | 测什么 | 对拍 | 结果 |
|------|--------|------|------|
| P01–P03 | prep / NTT / dot+encode 砖 | host/FIPS oracle | PASS_CPU |
| P04 | PKE 三 launch 全链 | liboqs_pke | PASS_CPU + **NPU×30** |
| K01 | KEM tail（H(ek)/z/dk） | host/K01 契约 | PASS_CPU+SIM + **NPU×30**（修 `__gm__` 字符串后） |
| K02 | KEM 四 launch 全链 | liboqs_kem | PASS_CPU + **NPU×30** |

细表：[`MATRIX.md`](MATRIX.md)。约束：[`COMMON.md`](COMMON.md)。
