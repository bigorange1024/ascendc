# FEEDBACK — DRW-K02-decaps-reenc

```text
ID: PASS
cmd: cd graph-tests/dec_related/RB-D05-decaps-reenc && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~8
sync_audit: clean(SYNC-05+SYNC-09)
notes:
  - 新建 RB-D05-decaps-reenc：reenc_prep_custom(AIV)→mid-sync→reenc_mix_custom(MIX)
  - I/O ek[1568]+m'[32]+r'[32]→c'[1568]；Host 禁预喂最终 c'/μ；BLOCK_DIM=1；flag 1/3+4
  - X12：prep 业务 GM 用 UB+DataCopy；pack DataCopy
  - CPU：c'≡liboqs_pke_ref Encrypt max=0；diag μ/u/v/t̂/Â/ŷ/yee/ρ 亦 max=0
  - 未抄 T22–T27 / alg14 / examples encrypt|encaps|decaps / frozen / l18；未改 Decrypt KB/DAG
  - 未跑 -r npu / SSH
next_hint: 请主控推 -r npu
```

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：双 launch；`c'≡liboqs_pke_ref`。
- **并行 NPU**（未等本机返回即上板；与 CPU 重叠）：侧树 `flock` `-r npu -v Ascend910B3`。

| 项 | 结果 |
|----|------|
| 对拍 | `c'` max=0 + diag μ/u/v/…；oracle=liboqs_pke_ref |
| 墙钟 | `wall_sec=3.255` / 180 |
| 结论 | **DG5 关**；开 **DRW-K03** FO |

ID: PASS_NPU  
exit: 0  
wall_sec: 3.255

## 细节

| 项 | 值 |
|----|-----|
| 代码目录 | `graph-tests/dec_related/RB-D05-decaps-reenc/` |
| 权威 | `scripts/liboqs_pke_ref encrypt`（`output/cross_backend.txt`） |
| kernel 墙钟 | ≈5.15s（预算 600） |
| sync_audit JSON | [`logs/sync_audit.json`](logs/sync_audit.json) |

## 主控补注（2026-09-09）

- **NPU×30 lean**：ok=30 fail=0；`ALL_DONE_K02_X30` @ 16:53:18。
