# F203 Alg.7 Phase A 向量化 — 技术总结

**定稿**：2026-06-23（Phase A 实验）；**单 poly 闭环**见 **[F203-Alg7-SampleNTT-单poly技术总结.md](F203-Alg7-SampleNTT-单poly技术总结.md)**（2026-06-24）  
**历史探针（已冻结 2026-06-28）**：[`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) — G+A+P+C 单 launch Phase A；tick 见 [`SIM_BENCHMARK.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/SIM_BENCHMARK.md)  
**活跃子轨**：单 poly [`pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg7-sample-ntt-k4/) · 16×`Â` [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines3-7-a-hat-k4/) · `s`/`e` [`pass-fix-f203-alg13-lines8-15-se-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-lines8-15-se-k4/)  
**讨论纪要**：[`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md`](../../qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md)

---

## 1. 问题

设备端生成 `a_hat[16,256]`（Alg.13 行 3–7 / Alg.7 SampleNTT），单 AIV，与母探针 V3 预采样同 launch。

---

## 2. 已验证结论

| 结论 | 证据 |
|------|------|
| 标量 PermuteChain A **功能正确** | CPU/SIM PASS，719237 tick |
| batch SHAKE128 **可对拍** | A-v1 PASS；SIM 仍慢于标量 A（+27%） |
| **rej 访问模式** 在 CAModel 上极敏感 | SetValue 版 +36%；GM 栈 +23% vs UB 直扫 |
| 48B 栈块 + compact/LUT **负优化** | vec_a +9%，vec_b +14% vs 881627 |
| Host 上 rej 占 ~5% 时间 | 设备上搬运+模拟器可放大 rej 成本 |
| **tiny_sha3 移植** 不比 PermuteChain 快 | SIM 更慢，已否决 |

---

## 3. 反模式

- UB `GetValue` / `SetValue` 作主路径（假向量）
- x86 `pshufb` LUT 字节直接当 cand 下标
- per-poly 重 absorb（应对齐 batch tail 续 squeeze）
- 用 **715537** 与 **881627** 混比（管线不同）

---

## 4. 推荐下一步

1. ~~**Alg.7 d1/d2 UB unpack POC**~~ → 见单 poly 总结（**已完成**）
2. **母探针 16-poly** 迁入 `alg7-vec-k4` 模块（SHAKE UB、Mins、interleave ROM）
3. **batch tail XOF**（16 路共享 squeeze while）
4. R5：**独立探针** 验证 Compare 掩码读法后再接 compact（勿在全链试错）

---

## 5. 参考实现

- 设备：`f203_a_hat_scalar.hpp`，`f203_a_hat_ub.hpp`，`f203_a_hat_rej_vec_{a,b}.hpp`
- Golden：`scripts/golden_a_hat_sampling.py`
- 业界：`thirdparty/liboqs/.../sampling.c`，`rej_uniform_avx2.c`，`rej_uniform_asm.S`
