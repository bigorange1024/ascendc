# STATUS — RB-D04-decrypt-full

| 字段 | 值 |
|------|-----|
| 刀 | DRW-D04 · E-D04-FULL / **Q-DEC-CORRECT** |
| 状态 | **PASS_CPU + PASS_NPU**（`Q-DEC-CORRECT` 已关） |
| 日期 | 2026-09-09 |
| 墙钟 | CPU 拼装 ~35 min；NPU kernel `wall_sec=2.652`（预算 180） |

## 目标达成

1. 单 binary 三核：`dec_prep_custom` / `dec_ntt_dot_custom` / `dec_intt_extract_custom`（basename 锁定）
2. Host：L1 → sync → L2a → sync → L2b → D2H `m[32]`（禁融单 launch）
3. Flag：L2a/L2b 各自 1/3+4；禁 5/7 SoftSync；X12 DataCopy
4. 头隔离：`d02_inc/` / `d03_inc/` 避免 tiling 撞名

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；m max=0；PASS_SYNC+PASS_IO；oracle=**liboqs_pke_ref** |
| `bash run.sh -r npu -v Ascend910B3` | SUCCESS（云机侧树）；m max=0；oracle=**liboqs_pke_ref**；`wall_sec=2.652` |
| 三 launch | Launch1/2a/2b + TRACE L2a/L2b GATE+handshake |
| sync_audit | clean（CPU 段） |

运营回报：[`../../decrypt-rebuild-ops/tasks/DRW-D04-decrypt-full/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-D04-decrypt-full/FEEDBACK.md)

## 加压

- NPU×30 lean/full：ok=30 fail=0（2026-09-09）
