# NTT 域 polyvec 内积探针（Â·ŝ）

**目录**：`pass-fix-f203-alg11-12-innerproduct-k4`  
**与 toy 关系**：[`pass-fix-f203-alg11-12-multiplyntts-k4`](../pass-fix-f203-alg11-12-multiplyntts-k4/) 验证单次 `MultiplyNTTs`；本探针验证 **Alg.13 行 18 内积部分**（无 ê）：

```text
t̂[p] = mod_q( Σ_{j=0}^{K-1} MultiplyNTTs(Â[p,j], ŝ[j]) )    p = 0..P_OUT-1
```

golden 对齐 `hat_inner_product_ref.c`（`HAT_MOD_SCALAR_I64`）。

---

## 1. GM 布局（唯一，与 alg13 一致）

**Â 为 K×K 行连续矩阵**（与 `input/a_hat.bin`、`hat_inner_product_add` 相同）：

```text
flat(p, j, c) = (p * K + j) * N + c

a_hat.bin : [P_OUT * S_VEC, N]  int32
s_hat.bin : [S_VEC, N]          int32
t_hat     : [P_OUT, N]          int32
```

头文件：`innerproduct_layout.h`（`a_hat_offset` / `s_hat_offset`）。

**禁止**再使用已废弃的 `a_col` 列主序 `(j*P_OUT+p)*N` 转置格式。

---

## 2. 实现（一期全 poly）

| 项 | 说明 |
|----|------|
| launch | 单 AIV、`blockDim=1` |
| 循环 | **外层 j、内层 p**（ŝ[j] 跨 p 复用） |
| Â 搬入 | 每 (p,j) 一次 `DataCopy` N 系数，`a_hat_offset(p,j)` |
| ∘ | `alg11_ub::compute_on_ub`（全 256） |
| ∑ | scratch `outLine` 向量 `Add` |
| final mod | `mod_q_final_vec` |

### scratch（`innerproduct_tiling.h`）

```text
fLoc @0 | row @256 | modT2 @512 | outLine @768
kScratchInts = 3*N + P_OUT*N
```

### 代码组件

| 组件 | 路径 |
|------|------|
| 布局 | `innerproduct_layout.h` |
| 主 kernel | `innerproduct_kernel.cpp` — `ProcessFullPoly` |
| ∘ | `multiplyntts-k4` → `alg11_ub::compute_on_ub` |
| final mod | `innerproduct_mod.hpp` |
| CPU ref | `innerproduct_ub_scalar.hpp` — `innerproduct_scalar_a_hat` |

---

## 3. 验收

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

| 模式 | 结果 | SIM tick（4×4×1，`a_hat` 行主序） |
|------|------|-----------------------------------|
| cpu | PASS | — |
| sim | PASS | **43992** |

历史 `a_col` 布局 ~40805 tick（已废弃）。详见 [SIM_BENCHMARK.md](SIM_BENCHMARK.md)。

半行双 AIV：[`innerproduct-k4-halfrows`](../pass-fix-f203-alg11-12-innerproduct-k4-halfrows/)

---

## 4. 集成路线（alg13 行 18）

1. 本探针 PASS ⇒ 设备在 **a_hat 行主序** 下内积正确  
2. `mixPass=4`：用 alg13 的 `a_hat.bin` + 管道 ŝ，调用**同一读法**  
3. 将 `ProcessFullPoly` 逻辑迁入 `stageHatInto`（+ê 单独一步）  

---

## 5. 编译宏

`INNERPRODUCT_P_OUT` / `INNERPRODUCT_S_VEC` / `ALG11_VEC_OPTS=1`（默认）

## 6. 二期 half

已冻结：[`frozen/.../halfbatch`](../frozen/frozen-pass-fix-f203-alg11-12-innerproduct-k4-halfbatch/)
