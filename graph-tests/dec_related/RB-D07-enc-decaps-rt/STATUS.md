# STATUS — RB-D07-enc-decaps-rt

| 字段 | 值 |
|------|-----|
| 刀 | DRW-K04 · E-K04-RT / **Q-RT-HANG** |
| 状态 | **PASS_CPU + PASS_NPU ×30 + 加压收口≈×100** |
| 日期 | 2026-09-09 |
| 墙钟 | CPU≈10.9s；NPU stress ≈2–3s/次（预算 180） |

## 验收

| 项 | 结果 |
|----|------|
| CPU | `K_dec≡K_enc≡liboqs` max=0 |
| NPU ×1 | SUCCESS（修 AllocSz 后） |
| NPU ×30 | **ok=30 fail=0** |
| NPU 加压 | 连续段约 93 + 重启后补 **7/7 fail=0** → **累计≈100**（中间用例；以后默认 ×30） |

运营：[`../../decrypt-rebuild-ops/tasks/DRW-K04-enc-decaps-rt/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-K04-enc-decaps-rt/FEEDBACK.md)
