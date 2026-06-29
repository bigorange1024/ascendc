# 行 18 内积 / 同步调试纪要（2026-06-12）

## 探针方法

- `HAT_DEBUG_INNER_ONLY=1`：注释 `+ê`、`final mod`，只写 lazy ∑ 到 `t_hat`
- `AscendC::printf`：`p=0,j=0` 的 `MultiplyNTTs` 输出；`p=0` lazy acc；`p=1,col133` 的 `+e/mod` 三段
- C golden：`scripts/print_inner_golden.py`

## 结论

| 阶段 | CPU | SIM（`dst_preset` / 全链路） |
|------|-----|------------------------------|
| `A[p,0]∘ŝ[0]` 首段 prod | 与 golden 一致 | 一致 |
| `p=0` lazy ∑（无 +ê/mod） | 与 golden 一致 | 一致 |
| `p=1` +ê、mod（col133） | pre_e/post_e/post_mod 与 golden 一致 | 全链路下与 CPU 一致 |

**内积本身在 SIM 上是对的**；此前误判来自探针 `p0_acc` 打印写在 `+e/mod` **之后**（打印的是已 mod 的值，不是 lazy acc）。

## 同步

- `HAT_LINE18_STRONG_SYNC=0`（仅 `PIPE_MTE3` + `PIPE_ALL`）：全链路 SIM `t_hat` **max_abs_diff=0**
- `HAT_LINE18_STRONG_SYNC=1`（加 `PIPE_MTE2`/`PIPE_V`）：同样通过  
→ **当前未复现「ŝ/ê 未同步导致内积错」**；S3 写 `dst` 后现有 barrier 足够。

## 当前验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 KERNEL_COMPUTE_BUDGET_SEC=120 bash run.sh -r sim -v Ascend910B4
```

调试开关见 `hat_debug_config.hpp`（默认全关）。
