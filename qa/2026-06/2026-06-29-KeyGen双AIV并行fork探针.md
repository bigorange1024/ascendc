# 2026-06-29 — KeyGen 双 AIV 并行 fork 探针

## 决策

- **T13h** 在 `fix-f203-alg13-device-keygen-k4-dual-aiv` 迭代；**不改** `pass-fix-f203-alg13-device-keygen-k4`。
- 首轮差分：`f203_keygen_prep_ub.hpp` 恢复 per-`blockIdx` 并行 `BuildAHat16ShardWithUb`。

## 验收（当日）

| 项 | 结果 |
|----|------|
| CPU 全链 | ✅ `output OK` |
| CPU `KEYGEN_VERIFY=1` | ✅ ek/dk PASS |
| SIM | ✅ total tick **542339**；`a_hat`/`src`/`ρ`/ek/dk 全 maxdiff=0 |
| golden 对拍 | 须 `write_keygen_bins` 或完整 `gen_data.py`；`KEYGEN_GOLDEN_ONLY=1` **不含** `golden_a_hat.bin` |

## 下一步

1. 本机 SIM + `KEYGEN_DEBUG_DUMP=1` 看 `a_hat` 行 8–15
2. 若 FAIL → `PIPE_SYNC_EVAL.md` P-02 / 对齐 `prep/ahat` 与 `pass-fix-f203-alg13-lines3-7-a-hat-k4`
3. SIM+KAT 过后再动 example

## 全仓自包含约束落地（追加）

- 定稿：[docs/engineering/用例自包含与设备全链约束.md](../../docs/engineering/用例自包含与设备全链约束.md)
- **活跃探针**：`vec-k4-v2` / `polyvec8` 的 `gen_data.py`、`mlkem_ref.py` 改本目录 `thirdparty/` + `scripts/`；`cross_check_ntt_study_c.py` 改本目录 `thirdparty/`
- **KeyGen**：`pass` / `exp` 同步 `mlkem_ref` 根路径、删除未用 `load_src_via_c_lib` / `host_ek_append.py`
- **example**：`exp-sepolyvec8-ntt-k8` vendored `thirdparty/ntt_onnx` + 修复 `mlkem_ref.py`
- **允许例外**：`library/shared` 编译期；仓库 `scripts/` CANN 壳；注释中的文档链接

## liboqs KAT（2026-06-29）

探针 [`fix-f203-alg13-device-keygen-k4-dual-aiv`](../../ascendc-tests/fix-f203-alg13-device-keygen-k4-dual-aiv/)：

```bash
cd ../../ascendc-tests/fix-f203-alg13-device-keygen-k4-dual-aiv
bash kat_liboqs_vs_ascendc.sh   # KAT_CPU_COUNT=10 KAT_SIM_COUNT=1 默认
```

| 项 | 结果 |
|----|------|
| CPU×10 | ✅ 同 SEED_D：`d=SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D=…")` ↔ liboqs `ml_kem_1024_keypair_derand` |
| SIM×1 | ✅ |
| 耗时 | ~390s（本机） |
| 种子示例 | CPU `[204299089, …, 3011020230]`；SIM `[581854400]`（见 `output/kat_liboqs_vs_ascendc.log`） |

liboqs：`thirdparty/liboqs` tag **0.15.0**（静态 `liboqs.a`）；`scripts/build_liboqs_pke_ref.sh` 已支持 `.a` / `.so`。

> **改名备注（2026-07-27）**：仓根全 PKE 构建现为 `scripts/build_liboqs_pke_ref_mlkem1024.sh`（源码/产物带 `mlkem1024` 后缀；旧 `build_liboqs_pke_ref.sh` 仍转发）。  
> 本条 KAT 实际多用用例内 `liboqs_pke_keygen_ref`；对应脚本现名 `build_liboqs_pke_keygen_ref.sh`。

## CPU `[SUCCESS]` 日志判读

**背景**：用户/KAT 见控制台或 `kat_liboqs_vs_ascendc.log` 中 prep 后约 6 行、`mmad` 后 3 行 `[SUCCESS][AIC_x]/[AIV_x]`，易误读为「2AIC+4AIV 多核占用/空转」。

**结论**（与家里 agent 对齐并写入定稿）：

| 点 | 说明 |
|----|------|
| 占核 | 两次 launch **串行**、**1 颗 AI Core**（`core_list=[0]`）；非整卡多核并行 KeyGen |
| SUCCESS 行 | **tikicpu 拓扑 artifact**；prep 真语义 **0 AIC + 2 AIV block** |
| 裁判 | WSL：**SIM** `profile_subtask_log*.toml`；实机再 **msprof** |
| KAT | liboqs 对拍 PASS 与 SUCCESS 行数 **无关** |

**定稿**：[docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md §4.1](../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md#41-cpu-successaic_x-与2aic4aiv误读) · **STATUS/PIPE**：pass + fix-dual-aiv §14

## Alg.14 Encrypt 探针迁入 ascendc-tests

**背景**：Encrypt 正确性验证最初误建在 `examples/incubating/exp-mlkem-f203-pke-encrypt-correctness-k4/`；用户明确应为 **ascendc-tests 探针**，非 examples 交付。

**结论**：

| 项 | 路径 |
|----|------|
| 活跃探针 | [`../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) |
| 路线 | **AscendC 积木拼装**（INTEGRATION_PLAN Launch 表）；**已删 liboqs** |
| G0 | marker launch 壳；`ENCRYPT_VERIFY=0` 默认 |
| 定型交付 | 另建 `exp-fips203-mlkem-pke-encrypt-k4` / `stable-*`（须 customspec） |

已更新：`../../ascendc-tests/INDEX.md`、`INTEGRATION_PLAN.md`、`STATUS.md`、`docs/notes/F203-KeyGen-exp交付示例技术总结.md` §5。  
待手动：`examples/incubating/INDEX.md` 删除 exp-encrypt-correctness 行（工具权限受限）。

## Alg.14 Encrypt G1–G4 设备拼装验收

**路线**：vendored 活跃探针多 launch 拼全链；**禁 liboqs**；golden 仅 `scripts/host_golden/`。

**G3 错误与修正（详 [`G3_SIM_AUDIT.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) §9）**：

| 问题 | 原因 | 修正 |
|------|------|------|
| 修正前 SIM 双 fake-Â | `t_dot_r` SIM 曾全零；未接 `g3_linear` | `u_hat` 改真 `at_r` |
| `g3_linear` 五参 launch | SIM `507000`（ACL internal error） | CPU 仍 `g3_linear`；SIM 不走五参 |
| `t_dot_r` launch | 同/独 session 均 `507000` | `tr_hat`：`pack_t_hat_as_at_r_col0` + `at_r` row0（数学等价） |

| Gate | 内容 | CPU/SIM（G3 修正后） |
|------|------|----------------------|
| G1 | ρ→a_hat + coins→r,e₁,e₂ | ✅ |
| G2 | NTT(r)→r̂ | ✅ |
| G3 | Âᵀ·r̂→û̂、t̂·r̂→tr̂ | ✅ CPU `g3_linear`；SIM 真 at_r + at_r col0；max=0 |
| G4 | INTT+噪声+μ+Compress₁₁/₅ pack→c | gate ✅ CPU+SIM；**c.bin VERIFY=1 仅 CPU ✅** |

### G4 复验（G3 修正后，2026-06-29）

| 模式 | verify_gate G1–G3 | ENCRYPT_VERIFY=1 c.bin | wall |
|------|-------------------|------------------------|------|
| CPU G4/G5 | max=0 | **max=0** 1568B | ~10s |
| SIM G4/G5 | max=0 | **FAIL** @382 | ~360–426s |

SIM 日志仍有 507000 + 末尾 `free(): invalid pointer`（G4 tail 待修，非 G3 回退）。

## Alg.14 Encrypt G5 推进与 SIM 全链阻塞（2026-06-29 晚）

**背景**：领导验收「整条 Encrypt 在 device」；默认 `ENCRYPT_GATE=5`；禁止 Host `t_hat.bin` staging。

**已完成**：

| 项 | 证据 |
|----|------|
| 设备 ByteDecode ek→t̂ | `prep/decode_ek/`；SIM t̂ max=0 |
| CPU G5 全链 | `ENCRYPT_VERIFY=1` → c.bin **max=0** ~10s |
| SIM G5 G1–G3 | `verify_gate` û/tr̂ **max=0** |
| SIM 双 session tr̂ | 同 session 第 2 次 at_r 错 → 改 `run_g3_at_r_device_once` 后 tr̂ max=0 |

**阻塞（明日汇报前须解）**：

| # | 现象 | 范围 |
|---|------|------|
| 1 | SIM `ENCRYPT_VERIFY=1` c.bin FAIL @382（0 vs 255） | G4 **与** G5 共用 G4 tail |
| 2 | 507000（未挡 gate 对拍） | 已知 runtime 债 |
| 3 | `free(): invalid pointer` 末尾崩溃 | 疑 ACL 多段 Init/Finalize |

**明日 demo 保底（CPU）**：

```bash
cd ../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4
ENCRYPT_VERIFY=1 ENCRYPT_GATE=5 bash run.sh -r cpu -v Ascend910B4
```

**家里 Agent 优先**：读 [`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md) §Encrypt · [`STATUS.md`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/STATUS.md) · [`G3_SIM_AUDIT.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) §9.11。

### G5 SIM 全链 PASS（2026-06-29 深夜）

| 项 | 结论 |
|----|------|
| 根因 | `g4_noise`/`pack` SIM launch **507000**（INTT 正常） |
| 修复 | device INTT×2 + host 标量 noise/pack（`f203_encrypt_g4_host_scalar.hpp`、`f203_encrypt_pack_host_scalar.hpp`） |
| 验收 | `ENCRYPT_VERIFY=1 ENCRYPT_GATE=5` SIM **max=0** ~367s |

