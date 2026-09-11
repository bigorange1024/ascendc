# STATUS — RB-T29-decaps-3launch

| 字段 | 值 |
|------|-----|
| 刀 | T29 · LR-DP-F2 |
| 状态 | **PASS_NPU×30** |
| 日期 | 2026-09-10 |
| 真机 | Ascend910B3 · ASCEND_DEVICE_ID=0（物理卡 5） |
| 证据 | `/mnt/workspace/launch-reduce-logs/t29-x30-20260910-052355.log`（ok=30 fail=0） |
| 冒烟 | `/mnt/workspace/launch-reduce-logs/t29-npu-smoke-20260910-052226.log` |

## 目标达成

1. Alg.21 Decaps：`dk`+`c` → `K[32]`；**3 launch**（`dec_decrypt` 融合 prep+NTT+INTT + `enc_prep` + `enc_compute`）
2. Flag：Decrypt 融合核内复用 **1/3**；prep 后 `SyncAll`（CrossCore Wait 环外）；Reenc **1/3+4**；`BLOCK_DIM=1`；永禁 5/7
3. NPU×30：全部 `c'==c` accept；`K≡liboqs Decaps`；magic `0x543F001D`

## 相对 T28

- 去掉独立 Host `dec_prep` launch
- `dec_decrypt_custom`：AIV0 `PrepUnpackDecrypt` → 全核 `SyncAll` → 原 NTT/INTT 两段握手

## 验收

| 项 | 结果 |
|----|------|
| NPU 冒烟 | **PASS**（accept + K 对拍 + TRACE） |
| NPU×30 | **PASS** ok=30 fail=0 |
