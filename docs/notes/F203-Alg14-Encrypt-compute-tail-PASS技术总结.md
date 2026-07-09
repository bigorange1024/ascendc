# FIPS 203 Alg.14 Encrypt — compute+tail PASS 探针技术总结

**探针**：[`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../../ascendc-tests/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/)  
**状态**：**PASS**（2026-07-08）；作为 **完整 Encrypt 实现的 compute+pack 基线**。

---

## 1. 数学契约（不变量）

在 **prep 素材已就绪**（host 提供 `y`、`â`（`a_hat`）、`ek`、`e₁`、`e₂`、`m`）前提下，设备实现 Alg.14：

```text
t̂ ← ByteDecode₁₂(ek)
ŷ ← NTT(y)
(û, tr̂) ← (Âᵀ | t̂ᵀ) ∘ ŷ          // kP=5 内积 + INTT k=8
u ← INTT(û) + e₁
e₂' ← e₂ + μ(m)   (mod q)          // Launch 前缀折叠，无单独 μ launch
v ← INTT(tr̂) + e₂'
c₁ ← ByteEncode₁₁(Compress₁₁(u))   // k=4 poly
c₂ ← ByteEncode₅(Compress₅(v))
c  ← c₁ ‖ c₂                        // 1568 B（ML-KEM-1024）
```

**未覆盖**：行 3–15（SampleNTT→Â、CBD→y/e₁/e₂）。

---

## 2. 可复用模式

### P-ECT-1：e₂+=μ 前缀折叠

在 INTT 前对 `e₂` GM 做 `μ ← Decompress₁(m)` 并模加，使行 21 的 `v ← INTT(tr̂)+e₂'` 与 tail **纯 pack** 兼容（tail 不再读 `m`）。

### P-ECT-2：SIM 单 launch 内联 pack

MIX 核 AIV 分支在 `u/v` 写 GM 后，按 subBlock 分片 pack：

- subBlock0 → c₁[0..1] + c₂  
- subBlock1 → c₁[2..3]  

实现：`f203_tail_pack_ops.hpp` → `tail_pack_shard_gm`。

### P-ECT-3：Compress 向量 + ByteEncode 标量 pack

见 [`F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](F203-ByteEncode-ByteDecode-d-向量与标量选型.md)。

---

## 3. 分核与 launch

| 路径 | 核型 | launch |
|------|------|--------|
| SIM | `KERNEL_TYPE_MIX_AIC_1_2`，blockDim=1，双 AIV | **1** |
| CPU | tikicpu 三 launch MIX 串行 + AIV pack | **4** |

INTT 输入须来自内积 **当次 UB**（`ProcessFromLocal`）；见 [`F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](F203-Encrypt-compute-行18-19-UB驻留技术总结.md)。

---

## 4. 验收与性能（910B4）

| 模式 | I/O | tick / 墙钟 |
|------|-----|-------------|
| SIM | c/u/v max=0 | **154781** |
| CPU | c/u max=0；v=golden | ~27s |

---

## 5. 后继：全链 Encrypt（已交付）

1. 全链设备探针：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/)（prep + 本探针，SIM **2 launch**）
2. **stable 交付**：[`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)（2026-07-09）
3. **验收权重**：无 NPU 时以 **SIM 为主参考**、CPU 为辅助 — 见 [`F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md`](F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)

---

## 附录 — 关键文件

| 文件 | 作用 |
|------|------|
| `compute/f203_encrypt_l18_l19_kernel.cpp` | SIM 融合核 |
| `compute/f203_tail_pack_ops.hpp` | tail pack 可复用例程 |
| `main.cpp` | host 编排；SIM 1 launch / CPU 4 launch |
| `scripts/gen_data.py` | golden（e₂+=μ + pack） |
