# ⛔ 已冻结（2026-06-16）

**原路径**：`ascendc-tests/fix-f203-2s1e-basemul-vec-k4/`  
**继任**：[`pass-fix-f203-alg11-12-multiplyntts-k4`](../../pass-fix-f203-alg11-12-multiplyntts-k4/) → 迁入 [`fix-f203-2s1e-alg13-16171820-k4`](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) 行 18

## 冻结原因

| 类别 | 说明 |
|------|------|
| **SIM 性能负收益** | 全链路 `mixPass=0`：标量基线 ~50s；B1 deinterleave+vec ~80s；B2 Gather+vec ~69s。**正确性 ✓，但比标量更慢 40–60%**。 |
| **混合 Barrett 未闭合** | 纯向量 Barrett final clamp 在 SIM 上 `t_hat` 爆炸（±2e9）；被迫每 lane 回退 `hat_reduce_zq_scalar` 收尾，向量优势被抵消。 |
| **标量 deinterleave 主导开销** | B1 用 `GetValue`/`SetValue` 解交错；B2 Gather 略快于 B1 仍慢于标量。热路径标量 UB 访问 + 过多 `PIPE` 同步，与 Alg11 `MEM_OPS=1` 结论相反。 |
| **全链路 spike 路线错误** | 在 `Aiv2s1eUbPipeline` 内「就地换 dispatch」未先 isolate 单次 `MultiplyNTTs`；缺少 `__gm__` ROM + Init `DataCopy`，γ 仍 128 次 `SetValue`。 |
| **被 Alg11 探针取代** | [`pass-fix-f203-alg11-12-multiplyntts-k4`](../../pass-fix-f203-alg11-12-multiplyntts-k4/) 同日后续交付：B2 Gather + 全向量 Barrett + **`ALG11_MEM_OPS=1`**（SIM tick ~−45% vs 慢路径）。行 18 迁入应以 **Alg11 模块**为基线，非本目录。 |

## 放弃决策（2026-06-16）

1. **不再维护** `HAT_BASEMUL_VARIANT` 1/2 与 `hat_basemul_variants.hpp` 路线。  
2. **不再参考** 本目录作 basemul 向量化的上游或 fork 基线。  
3. **行 18 下一跳**：从 `pass-fix-f203-alg11-12-multiplyntts-k4` 复制 `alg11_rom_tables` / `multiply_ntts_vec.hpp` / `alg11_ub_load.hpp` 等，嵌入 `stageHatInto` 的 `j` 循环；保持 `Aiv2s1eUbPipeline` 单 TPipe 融合。  
4. **ByteEncode 向量**仍走 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../../pass-fix-f203-2s1e-byteencode12-vec-k4/)（与本目录无关）。

## 历史价值（只读）

- 首次在 **2s1e 全链路**内尝试 post-NTT Gather 解交错 + 向量 `Mul`/`Add`；证明「能跑」≠「该合并」。  
- 记录 SIM 上向量 Barrett 收尾踩坑（见 [BASEMUL_VEC.md](BASEMUL_VEC.md) §踩坑）。  
- `BYTE_ENCODE12_VEC` 与 `2s1e_post_ntt_ub.hpp` scratch 增量公式 → byteencode fork 仍可用。

**勿 fork、勿抄码、勿跑 CI。**
