# F203 KeyGen 交付示例（exp-fips203-mlkem-pke-keygen-k4）— 技术总结

**读者**：需要从探针晋级为 **自包含交付示例**、或 fork 为 Encrypt 等下一用例的实现者  
**定型交付（2026-06-29）**：[`examples/stable/stable-fips203-mlkem-pke-keygen-k4/`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) — 验收以 stable 为准。

**案例锚点（incubating 副本）**：[`examples/incubating/exp-fips203-mlkem-pke-keygen-k4/`](../../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/)  
**探针对照**：[`pass-fix-f203-alg13-device-keygen-k4`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/)（同生产 I/O）  
**讨论**：[`qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md`](../../qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md) §6  
**实现方案**：[`exp-fips203-mlkem-pke-keygen-k4-实现方案-customspec.tex`](../../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/exp-fips203-mlkem-pke-keygen-k4-实现方案-customspec.tex)

---

## 1. 交付语义

FIPS 203 **Alg.13 PKE KeyGen**（ML-KEM-768，\(K=4\)）：

- **输入**：`input/seed_d.bin` + NTT LUT（`lut_even/odd_stacked.bin`）
- **输出**：`output/ek_pke.bin`（1568B）+ `output/dk_pke.bin`（1536B）
- **设备**：2 launch — prep（AIV×2）→ compute MIX（`F203_KEYGEN_EK_PKE=1`，行 21 ek‖ρ 内核融合）

**与探针差异**：exp **无** G0–G4 门禁、无 `compute_io/` staging、无分段 Host 入口；`run.sh` CMake 宏 **硬编码**（见 customspec §唯一路径）。

---

## 2. 自包含目录契约

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`compute/`、`cmake/keygen/`、`scripts/prep/`、`scripts/compute/` | `ascendc-tests/` include/import |
| `library/shared/`（SHAKE、Keccak） | 运行时依赖 `thirdparty/liboqs`（KAT 脚本除外） |
| vendored `thirdparty/ntt_study/`（**Host LUT golden 表**） | 把 ntt_study **docs/specs** 当作本仓 CPU/设备切分规范 |

中间 GM（`a_hat`、`src`、ρ 等）**不落盘**；`KEYGEN_DEBUG_DUMP=1` 仅调试。

---

## 3. 验收阶梯

```bash
cd examples/incubating/exp-fips203-mlkem-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4   # Host golden
bash kat_liboqs_vs_ascendc.sh                     # 10 CPU + 1 SIM vs liboqs
```

**SIM 指标**（`run.sh -r sim` 控制台）：

- Launch1 prep tick / wall_ms  
- Launch2 compute tick / wall_ms  
- `SIM summary: total_tick=… total_wall_ms=…`

解析逻辑：[`scripts/parse_keygen_sim_metrics.py`](../../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/scripts/parse_keygen_sim_metrics.py)（优先 `profile_task_log0.toml`）。

**典型 tick**（Ascend910B4）：prep ≈462k + compute ≈80k → total ≈**542393**。

---

## 4. 工程不变量（复用清单）

| 主题 | 规则 | 详见 |
|------|------|------|
| SIM 环境 | build → `sim_env_export` → kernel | qa 2026-06-28 §1；[`scripts/sim_env.sh`](../../scripts/sim_env.sh) |
| 双 AIV SHAKE | 内嵌 `ProcessInline`，勿用外层 `GetBlockIdx()` 分 batch | [F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md](F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) |
| Pipe 同步 | CBD 合入前须 barrier；勿为 tick 删未证 barrier | [F203-KeyGen-prep-Pipe细同步技术总结.md](F203-KeyGen-prep-Pipe细同步技术总结.md) |
| compute 向量 | `mixPass=0`、`HAT_ALG11_VEC=1`、`BYTE_ENCODE12_VEC=1` 等 | customspec §唯一路径 |
| KAT 超时 | `KEYGEN_KERNEL_BUDGET_SEC≥1200` | `run.sh` |

**prep 双 AIV 并行 Â**（2026-06-29）：已与 pass/stable 对齐；见 [F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md](F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)。

---

## 5. fork 下一用例（PKE Encrypt）

**正确性验证**（已建）：[`fix-f203-alg14-pke-encrypt-correctness-k4`](../../ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) — **AscendC 多 launch 拼装**探针（G0 壳）；**禁 liboqs 生产路径**。

**定型交付**（验证通过后另建）最小步骤：

1. **rsync** KeyGen stable → `exp-fips203-mlkem-pke-encrypt-k4`（或晋级 `stable-fips203-mlkem-pke-encrypt-k4`）。  
2. **先写 customspec**：I/O、Launch、CPU/设备表；**勿**继承 ntt_study 目标 4 的算子切分。  
3. 改 Host golden / `prepare_production_input.py`；保留 `run.sh` 编排模式。  
4. compute 增量（Encrypt 需 INTT、Compress 等）在 vendored `compute/` 内 fork，仍保持 **唯一路径**。  
5. 验收对齐：CPU → SIM → liboqs KAT；note + qa 当日追加。

---

## 6. 案例附录

| 文件 | 作用 |
|------|------|
| `main_keygen.cpp` | 唯一 Host：GM 布局、2 launch、output scrub |
| `f203_keygen_prep_entry.cpp` / `f203_keygen_prep_ub.hpp` | Launch1 编排 |
| `compute/mmad_custom.cpp` | Launch2：2s1e + 内积 + ByteEncode + ek‖ρ |
| `scripts/keygen_golden.py` | Host 全链 golden |
| `kat_liboqs_vs_ascendc.sh` | 外部 KAT 入口 |
