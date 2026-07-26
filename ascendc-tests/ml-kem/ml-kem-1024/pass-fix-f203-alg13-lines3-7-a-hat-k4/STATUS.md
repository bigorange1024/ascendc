# STATUS — pass-fix-f203-alg13-lines3-7-a-hat-k4

**语义**：FIPS 203 **Alg.13 行 3–7** — 设备生成 **`Â` / `a_hat[16,256]`**（k=4）；**不含**行 8–15、NTT、行 18。

| 阶段 | CPU | SIM | 说明 |
|------|-----|-----|------|
| G0 golden | ✅ | ✅ | `scripts/gen_data.py`；`max_abs_diff=0` |
| G1–G3 设备链 | ✅ | ✅ | 16×（逐 poly SHAKE + 向量 d12/rej）；链末写 GM |
| G4 batch16 SHAKE | ⚠️ | ⚠️ | SIM 对齐已修（`kShakeMsgStride=64`）；tick **960098** vs 逐条 **733859**，**默认关闭** |
| G5 双 AIV | CPU ✅ | SIM ✅ | 默认 `F203_AHAT16_BLOCK_DIM=2`；SIM **381544** tick（−46.7% vs 单 AIV）；SHAKE 内嵌须 `ProcessInline` |
| G6 504B XOF | ✅ | ✅ | `F203_ALG7_XOF_504=1`：SIM **549224** tick（**−25.2%** vs 672）；504 路径暂用标量 rej |

**SHAKE 路径**：默认 `F203_AHAT16_BATCH_SHAKE=0`（16×逐条）；`BATCH_SHAKE=1` 已修 SIM 对齐（x 行 stride 64B）但 tick **负优化**，见下表。

**验收（2026-06-24）**

```bash
bash run.sh -r cpu -v Ascend910B4
KERNEL_COMPUTE_BUDGET_SEC=600 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 504B 对照（非默认；SEED_D=20260619 与 672B golden 一致）
F203_ALG7_XOF_504=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# 2 AIV 对照
F203_AHAT16_BLOCK_DIM=2 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | SHAKE | tick / 墙钟 | 对拍 |
|------|-------|-------------|------|
| CPU | 逐条（默认） | — | `a_hat PASS` |
| SIM | 672B 1 AIV vec rej（**默认**） | **733859** / ~333s | `max_abs_diff=0` |
| SIM | 504B 1 AIV（标量 rej 对照） | **549224** / ~263s | PASS；**−25.2% tick**；`F203_ALG7_XOF_504=1` |
| SIM | 2 AIV 672B | **714150** / ~334s | PASS；tick −2.7% 但墙钟持平（见下） |

**2 AIV 读数**：SIM tick **非**关键路径——714k ≈ 2×(733k/2)；墙钟 333s→334s 无加速。架构上仍合理；并行收益待真机墙钟。详 [`INTEGRATION_PLAN` §5.1](INTEGRATION_PLAN.md)、qa §7.2。

**性能原则**：Â 生成为 **Keccak 计算密集**；UB 内 SHAKE→d12→rej，**链末写 GM**；与 NTT/内积分 launch。

**上游模块**：[`pass-fix-f203-alg7-sample-ntt-k4`](../pass-fix-f203-alg7-sample-ntt-k4/)（单 poly PASS，SIM ~80100 tick）

**定稿（单 poly）**：[docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md)

**讨论**：`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md`；`qa/2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md`（§7.3 504B）
