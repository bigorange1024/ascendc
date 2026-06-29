# F203 Stage3 RouteA + 取模：三种实现记录

日期：2026-06-11（2026-06-12 注：本用例为 **limb 面对半** 历史对照；MLKEM 新实现须用 `fix-f203-tag5t-ntt256-limb6-poly8-polybatch-s123` 之 `AivTag5tRouteAModPolyBatch`，见 [MLKEM-NTT-实现总结](../../docs/notes/MLKEM-NTT-实现总结.md)）  
用例：`fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123`  
golden：**固定** `f203_stage3_route_a` + `stage31_mod`（`scripts/gen_data.py` / `mlkem_ref.py`），**不可改**。

公共前置（三种方案相同）：

- `AivTag5tRouteAMod`：GM `DataCopy` 整行 + `Gather` 解交织 → `hh/lh/hl/ll`
- Horner 合并（方案 0/1/2 中 1、2 显式 raw；方案 0 在 Barrett 步中融合）

切换宏：`stage3_config.hpp` → `F203_STAGE3_MOD`（0/1/2）  
实现归档：`stage3_mod_variants.hpp`（由 `ntt_vec.hpp` include）

```bash
# 验证
bash run.sh -r cpu
bash run.sh -r sim
TAG5T_MIX_PASS=3 bash run.sh -r cpu   # 仅 Stage3
```

---

## 对照表

| 宏 | 名称 | 拓扑语义 | 取模手段 | CPU | SIM 全链路 | UB 要点 |
|----|------|----------|----------|-----|------------|---------|
| **0** | Barrett 三步 | 合并时逐步约化（非 ONNX 两阶段） | int32 Barrett μ=314,k=20 | ✓ | ✓ ~10.5s | `scratch_t1/t2` |
| **1** | Scalar I64 | Horner raw + stage31_mod | `GetValue` int64 `/` | ✓ | ✓ ~16s | `scratch_t1`；SIM 超时设 20s |
| **2** | Cast+Div | Horner raw + ONNX Div→Mul→Sub | float `Div` + int32 Muls/Sub | ✓ | ✓ ~10s | `scratch_t1` + `calc_f`×3 float |

---

## 方案 0：Barrett 三步 Horner

**思路**：`MlkemCombineReduceLimb6Barrett` 向量版——每乘一次 `×64` 并加上 limb 后立即 Barrett，**不是**「先 raw 再一次 mod」。

**代码**：`stage3_mod_variants.hpp` → `combine_limb6_horner_barrett_vec`

```cpp
DataCopy(dst, hh, count);
barrett_reduce_limb6_vec(dst, q, t1, t2, count);
Add(t1, hl, lh, count);
ShiftLeft(dst, dst, 6, count);
Add(dst, dst, t1, count);
barrett_reduce_limb6_vec(dst, q, t1, t2, count);
ShiftLeft(dst, dst, 6, count);
Add(dst, dst, ll, count);
barrett_reduce_limb6_vec(dst, q, t1, t2, count);
```

**aiv_func**：仅需 `scratch_t1`、`scratch_t2`（可去掉 `calc_f`）。

**优点**：全程 int32 向量，无 Cast/Div；与本 golden **数学等价**；SIM 快。  
**缺点**：与 ntt_study/ONNX「Stage3 raw + Stage3.1 Div」拓扑不一致（审计/对图时用方案 2）。

---

## 方案 1：Horner raw + 标量 Stage31ModI64

**思路**：向量 Horner 出 `raw`，再按 `exp-mlkem-f203-stage3-routea-mod-vec` 的 `Stage31ModI64` 逐元素 floor mod。

**代码**：`combine_limb6_routea_mod_scalar_i64` / `stage31_mod_i64_scalar`

```cpp
const int64_t t = (raw >= 0) ? (raw / q64) : (-((-raw) / q64));
rem = raw - q64 * t;
```

**aiv_func**：仅需 `scratch_t1`。

**优点**：与 golden / ntt_study 语义最直白；不依赖 float API。  
**缺点**：128×8 标量循环，**PEM SIM 极慢**（~16s）；真机上也逊于向量方案。

---

## 方案 2：Cast + float Div + int32 Muls/Sub（当前默认）

**思路**：ONNX Stage3.1——`rem = raw - q * trunc(raw/q)`；910B **无 int32 向量 Div**，仅 float。

**代码**：`combine_limb6_routea_mod_cast_div` / `stage31_div_mod_vec`

```cpp
Cast(fRaw, dst, CAST_NONE, n);           // int32 raw → float
Duplicate(t1, q, count);
Cast(fTmp, t1, CAST_NONE, n);            // int32 q → float 向量（勿 Duplicate(float,q)）
Div(fQuot, fRaw, fTmp, count);
Cast(t1, fQuot, CAST_TRUNC, n);          // 商 → int32
Muls(t1, t1, q, count);
Sub(dst, dst, t1, count);                // int32 域减（勿 float Sub）
```

**aiv_func**：`scratch_t1` + `TBuf<VECCALC> calc_f`（3×`halfLen` float：`fRaw/fTmp/fQuot`）。

**优点**：对齐 ONNX/ntt_study 两阶段拓扑；向量为主，SIM ~10s。  
**缺点**：比 Barrett 多 Cast/Div；大 `|raw|>`2²⁴ 时 float 商可能差 1（本 testcase 已验证 0 mismatch）。

---

## 踩坑清单

### 1. `ReinterpretCast` ≠ 数值转换

`int32` buffer `ReinterpretCast<float>()` 后是 **bitcast**，不能当 `3302.0f` 去除。必须用 `Cast`。

### 2. 910B `Div` 仅 half/float

[Div API](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0038.html) 在 A2/910B 上不支持 int32。`Divs(int32)` 亦无。

### 3. `Duplicate(float, q)` 在 CPU sim 不可靠

表现为 `fTmp≡0` → Div 除零 → 最终 **dst 全 0**。改法：`Duplicate(t1, q)` + `Cast(fTmp, t1)`。

### 4. float `Sub` 在 CPU sim 不可靠

`Sub(fRaw, fRaw, fTmp)` 或 `Sub(fQuot, fRaw, fTmp)` 曾输出全 0。改法：**商 Cast 回 int32 后 `Muls/Sub` 在 int32 域完成**。

### 5. VECIN `TQue` 超过 8 个 → `AllocEventID` 失败

曾用 4 个 float `TQue` 导致 SIM/CPU **静默全 0**。改法：float scratch 合并到 **一个 `TBuf<VECCALC>`** + `GetWithOffset`。

### 6. `dst` 复用为 `fQuot` 后不宜 `Cast` 回 int32

`dst.ReinterpretCast<float>()` 写过商后，直接 `Cast<int32>(dst, fRem)` 曾异常。现方案最终 `Sub` 写回 int32 `dst`，无此问题。

### 7. int32→float 勿经 `half` 中转（本数据）

Horner `raw` 可达 **3e8 量级**，`half` 会溢出/归零。必须 **int32→float 直接 Cast**。

### 8. 单次 Barrett 对完整 raw ≠ golden

「Horner 一次出 raw 再单次 Barrett」与三步 Barrett **不等价**；与 `stage31_mod` 也不等价。

### 9. `KYBER_PIPE_ALL` / `PipeBarrier`

CPU debug 下宏为空；真机 SIM 上 Cast/Div 链建议保留 barrier。注释掉后本 testcase CPU/SIM 仍 PASS，真机需自测。

### 10. SIM 超时

方案 1 全链路 ~16s，`run.sh` 里 `KERNEL_COMPUTE_BUDGET_SEC` 需 **≥20**（方案 0/2 ~10s，15s 够用）。

---

## 快速切换步骤

1. 编辑 `stage3_config.hpp`：

```cpp
#define F203_STAGE3_MOD 0   // 或 1、2
```

2. 确认 `aiv_func.hpp` 中 `Init`/`Compute` 与宏一致（已 `#if` _guard）：
   - `F203_STAGE3_MOD == 2`：保留 `calc_f` TBuf
   - `else`：仅 `scratch_t1`（方案 0 还需 `scratch_t2`）

3. `bash run.sh -r cpu && bash run.sh -r sim`

---

## 参考

- golden：`examples/incubating/exp-sepolyvec8-ntt-k8/scripts/mlkem_ref.py`（`stage31_mod`）
- 标量参考：`examples/incubating/exp-mlkem-f203-stage3-routea-mod-vec/f203_stage3_routea_mod_custom.cpp`
- Barrett 表：`thirdparty/ntt_study/include/mlkem/stable/mlkem_ntt_tables.h`（μ=314, K=20）
- ntt_study Div 讨论：`thirdparty/ntt_study/qa/2026-05/讨论记录_2026-05-11__问题定位__Div-RealDiv-取模链与代码调查.md`
