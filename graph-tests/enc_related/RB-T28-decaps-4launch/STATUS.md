# STATUS — RB-T28-decaps-4launch

| 字段 | 值 |
|------|-----|
| 刀 | T28 · LR-DP-F1 |
| 状态 | **PASS_NPU×30** |
| 日期 | 2026-09-10 |
| 真机 | Ascend910B3 · ASCEND_DEVICE_ID=0（物理卡 5） |
| 证据 | `/mnt/workspace/launch-reduce-logs/t28-x30-20260910-131020.log`（ok=30 fail=0） |

## 目标达成

1. Alg.21 Decaps：`dk`+`c` → `K[32]`；**4 launch**（dec_prep + dec_ntt_intt 融合 + enc_prep + enc_compute）
2. Flag：Decrypt 融合核内复用 **1/3**；Reenc **1/3+4**；`BLOCK_DIM=1`；永禁 5/7
3. NPU×30：全部 `c'==c` accept；`K≡liboqs Decaps`；magic `0x543F001C`

## 关键修复（上板关键）

T26 把 T25 的 NPU 安全路径回退成 `GlobalTensor::SetValue` 镜像 dk/c 与写 m → **CPU 假绿、NPU m' 全错**（同输入 CPU accept / NPU reject）。
T28 回迁 T25 修法：prep 直读 H2D + DataCopy；m 经 UB DataCopy 写出；再融合 NTT+INTT。

## 验收

| 项 | 结果 |
|----|------|
| NPU 冒烟 | **PASS**（accept + K 对拍） |
| NPU×30 | **PASS** ok=30 fail=0 |
