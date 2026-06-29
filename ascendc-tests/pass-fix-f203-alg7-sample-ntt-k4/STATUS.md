# STATUS — pass-fix-f203-alg7-sample-ntt-k4

**语义**：FIPS 203 **Alg.7 SampleNTT** — **单次**生成一个 poly 系数 `â[256]`（给定 `SEED_D` 与 `(j,i)`）；非 16×4 `Â` batch。

| 阶段 | CPU | SIM | 说明 |
|------|-----|-----|------|
| Phase G → ρ | ✓ | ✓ | 标量 SHA3（`f203_alg7_g.hpp`） |
| Alg.7 line5 SHAKE128 → **672B** | ✓ | ✓ | 固定 4×rate（=504+168）；`shake_xof_kernel` UB I/O |
| 解交织 + 向量 d1/d2 | ✓ | ✓ | `DeinterleaveCandFromUb`；**224 pair**；Gather 实验可选 |
| **rej vec_mins（生产默认）** | ✓ | ✓ | `F203_ALG7_REJ_IMPL=1` |
| **rej vec_mask（实验）** | ✓ | ✓ | `F203_ALG7_REJ_IMPL=2` |
| **rej scalar（对照）** | ✓ | ✓ | `F203_ALG7_REJ_IMPL=0` |
| golden 对拍 | ✓ | ✓ | d1/d2[224] + **a_hat**（xof 默认不 dump） |

**XOF 672B 策略**：不做 lazy tail while；见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §1.4、`f203_alg7_layout.h` 文件头。

**SIM tick（Ascend910B4，blockDim=1）— 504 vs 672 对照**

| XOF 长度 | scalar rej tick | Δ | 说明 |
|----------|-----------------|---|------|
| **504B**（3×rate，历史） | **~55738** | — | 单块；~1% 种子统计上需 tail |
| **672B**（4×rate，**当前默认**） | **~63213–63265** | **+~13%** | 多 squeeze 1×168B Keccak；**非** rej 向量带来 |

**SIM tick — rej 剔除双方案（672B，2026-06-23）**

| `F203_ALG7_REJ_IMPL` | 剔除原语 | SIM tick | Δ vs scalar |
|----------------------|----------|----------|-------------|
| **0** `scalar` | 标量 GetValue | **63256** | — |
| **1** `vec_mins`（**默认**） | `Mins(d,q)` ×2 | **63222** | **−34（≈0%）** |
| **2** `vec_mask` | `Compares(LT)+Select` | **63249** | **−7** |

剔除段已无 `GetValue`；compact 仍标量。**结论**：两向量剔除方案 tick 与 scalar 同量级，**Mins 略优于 Compares+Select**；整体瓶颈仍在 XOF + compact。

**全链 SIM tick（默认 vec_mins，含 SHAKE+d12+rej+GM）**：**~80100**（2026-06-24）；较仅 rej 段 ~63222 高约 17k，因单 TPipe 全链与 ROM Init 等。

**R5 向量 compact**：多轮 SIM 未通过（`Compares`/`Compare` 掩码）；已回退标量；见 [`docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md`](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md) §5。

**性能结论**：672 的收益在 **固定块长 / 无 tail 分支 / 向量管线**，**不**在 SIM tick 下降；增量主要来自多挤 168B XOF（Host 上 XOF ~95% 时间）。

**阶段策略（用户 2026-06-24）**：当前为**实现原型**，672B **保险**可接受；若后续要优化 tick，可收紧为 **504B + lazy tail(168)**，或母探针上 **batch XOF 定长** 另议——见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §1.5、qa §17。

**范围**：单 poly `(j,i)`；line 8–15 对 **448 lane** 批量 rej + 取前 256。**功能验收可称完成**（I/O 等价 POC）；非 FIPS 字面 lazy tail / 非 batch `Â`。

**验收（默认 `F203_ALG7_REJ_IMPL=1`）**

```bash
cd ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4
python3 scripts/gen_data.py
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 标量对照：F203_ALG7_REJ_IMPL=0 bash run.sh -r sim -v Ascend910B4
python3 scripts/test_multi_seed.py
```

**定稿**：[docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md)

**实现方案**：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)

**讨论**：`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md` §16–§18；`qa/2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md`
