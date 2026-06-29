# 行 18 basemul 向量化探针（frozen-fix-f203-2s1e-basemul-vec-k4）

⛔ **已冻结（2026-06-16）** — 见 [FROZEN.md](FROZEN.md)。**勿参考、勿 fork。** 继任：[`pass-fix-f203-alg11-12-multiplyntts-k4`](../../pass-fix-f203-alg11-12-multiplyntts-k4/)。

**更新**：2026-06-15（当晚 spike）

基于 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)：仅替换 `multiply_ntts_half_scalar` 为可切换变体；ByteEncode 保持向量实现。

层 C（改 \(\hat{A}\) GM 布局）**未做**。

---

## 变体（`HAT_BASEMUL_VARIANT`）

| 值 | 名称 | 做法 |
|----|------|------|
| **0** | 标量基线 | `multiply_ntts_half_scalar` |
| **1** | B1 无 Gather | 标量 deinterleave → 向量 `Mul`/`Add` → **混合 Barrett**（向量前两步 + 标量 `hat_reduce_zq_scalar` 收尾）→ 标量 interleave |
| **2** | B2 Gather | `Gather` 解交错 → 同 B1 向量核 |

```bash
cd ascendc-tests/frozen/frozen-fix-f203-2s1e-basemul-vec-k4
HAT_BASEMUL_VARIANT=0 bash run.sh -r cpu -v Ascend910B4
HAT_BASEMUL_VARIANT=1 bash run.sh -r sim -v Ascend910B4
```

---

## 验收（2026-06-15）

| 变体 | CPU | SIM | 备注 |
|------|-----|-----|------|
| 0 | ✓ | ✓ | 基线 |
| 1 | ✓ | ✓ | 见下「踩坑」 |
| 2 | ✓ | ✓ | Gather 在 post-NTT 可用；SIM 正确性 OK |

---

## SIM 墙钟（全链路 mixPass=0，单次粗测）

| `HAT_BASEMUL_VARIANT` | 时间 |
|------------------------|------|
| 0 标量 | **~50 s** |
| 1 deinterleave+vec | ~80 s（**更慢**） |
| 2 gather+vec | ~69 s（**更慢**） |

结论：当前 spike **未带来性能收益**；额外 deinterleave/Gather + 混合 Barrett + 更多 `PIPE` 在 SIM 上得不偿失。真 NPU 待测。

---

## 踩坑（已验证，明天讨论素材）

### 1. 纯向量 Barrett final clamp → SIM `t_hat` 爆炸

向量 `Muls`/`ShiftRight`/`Sub` 做 Barrett 前两步后，用向量式 `x - (q & ~((x-q)>>31))` 收尾，SIM 上 `t_hat` 出现 `±2e9` 级错误；CPU 仍 PASS。

**修复**：`hat_reduce_zq_vec_barrett` 末尾对每个 lane 调 `hat_reduce_zq_scalar`（与 C ref 完全一致）。

### 2. 6/12 历史问题再现

向量 `Mul`/`Add` 与标量 `GetValue`/`SetValue` 混用时，**PIPE 同步不足**会在 SIM 暴露；CPU 顺序执行掩盖。本 spike 通过标量 reduce 收尾 + 额外 `KYBER_PIPE_ALL` 绕过，但未做性能优化。

### 3. 纯向量 Barrett（未做成）

层 A「全向量 Barrett」在 SIM 上**未验通**；未继续硬啃，避免烧 token。

### 4. 层 A 其它项（未单独实现）

- 减 `stageHatInto` 外层 `PIPE`（未做 A/B 对照）
- `pairCount=128` 单次半 poly（UB 够但未测）
- γ `Duplicate` 向量加载（仍 64 次 `SetValue`）

### 5. 层 C

按约定跳过。

---

## 关键文件

| 文件 | 作用 |
|------|------|
| `basemul_config.hpp` | `HAT_BASEMUL_VARIANT`、scratch 增量 |
| `hat_basemul_variants.hpp` | B1/B2 实现 + `multiply_ntts_half_dispatch` |
| `2s1e_post_ntt_ub.hpp` | scratch +11×`pairCount`；调用 dispatch |

---

## 明天讨论建议

1. **正确性**：SIM 上向量 Barrett 为何需回退标量 reduce？是否 `ShiftRight` 语义 / 有符号中间量？
2. **性能**：SIM 变慢主因是 deinterleave 标量循环还是 barrier 过多？应用 profiler 只看行 18。
3. **下一步**：真向量 Barrett 修通前，是否值得 **减标量 deinterleave**（例如交错布局上直接算交叉项）？
4. **Gather**：B2 比 B1 快 ~11s 但仍慢于标量 — Gather 解交错是否值得？

讨论纪要可追加：[qa/2026-06/2026-06-15-ByteEncode12向量与Scatter讨论.md](../../qa/2026-06/2026-06-15-ByteEncode12向量与Scatter讨论.md) §8。
