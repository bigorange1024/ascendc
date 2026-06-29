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
- **example**：`exp-sepolyvec8-ntt-k8` vendored `thirdparty/ntt_study` + 修复 `mlkem_ref.py`
- **允许例外**：`library/shared` 编译期；仓库 `scripts/` CANN 壳；注释中的文档链接

## liboqs KAT（2026-06-29）

探针 [`fix-f203-alg13-device-keygen-k4-dual-aiv`](../../ascendc-tests/fix-f203-alg13-device-keygen-k4-dual-aiv/)：

```bash
cd ascendc-tests/fix-f203-alg13-device-keygen-k4-dual-aiv
bash kat_liboqs_vs_ascendc.sh   # KAT_CPU_COUNT=10 KAT_SIM_COUNT=1 默认
```

| 项 | 结果 |
|----|------|
| CPU×10 | ✅ 同 SEED_D：`d=SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D=…")` ↔ liboqs `ml_kem_1024_keypair_derand` |
| SIM×1 | ✅ |
| 耗时 | ~390s（本机） |
| 种子示例 | CPU `[204299089, …, 3011020230]`；SIM `[581854400]`（见 `output/kat_liboqs_vs_ascendc.log`） |

liboqs：`thirdparty/liboqs` tag **0.15.0**（静态 `liboqs.a`）；`scripts/build_liboqs_pke_ref.sh` 已支持 `.a` / `.so`。

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

## 根目录幽灵探针目录清理（2026-06-29）

误建路径（自包含 vendoring 脚本漏写 `ascendc-tests/` 前缀，`mkdir -p` 仅留下空 `thirdparty/.../stable/`）已删除：

- `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/`（仓库根，**非**探针）
- `pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`（仓库根，**非**探针）

活跃探针仍在 `ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/`、`ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`。

## 交接与 GitHub（2026-06-29 晚）

- 删除 `HOME-KEYGEN-DEBUG.md`；**`AGENT_HANDOFF.md` 改为每日刷新**（办公室 ↔ 家里唯一短交接）。
- `.gitignore`：`thirdparty/` → `/thirdparty/`（仅忽略仓库根 liboqs；**探针/example 内 vendored `thirdparty/ntt_study` 须进 Git**）。
- 家里 Agent：**优先 `git pull`**，勿依赖旧 `backup/v0.1_20260626*`（缺 scripts、stable、vendored LUT）。
