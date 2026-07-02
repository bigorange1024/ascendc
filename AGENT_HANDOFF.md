# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-07-02（**Alg.21 Decaps 单设备库合并 · CPU 单 session PASS**；SIM 单 session + `nm` 待公司验证 · 详 [`qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md`](qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) §7）

---

## ★ 下一任务（家里 Agent · P0）

### 1. Alg.21 Decaps — SIM 单 session 验证 + `nm` 审计（承家里合库）

**家里已做（2026-07-02）**：**根因定位 + 单设备库合并 + CPU PASS**。
- 根因修正：SIM 单 session 重加密 `c' max=244` **不是**「泛化 CAModel 污染」，而是探针曾用 **decrypt/encrypt 双设备库**在一个 ACL session 内 **func_key 空间重叠 / 装载边界冲突**。本仓过关 SIM 探针皆单库；decaps 唯一双库=差异变量。
- 修法：合并单库 `ascendc_kernels_${RUN_MODE}`；双树同名头仅 `aiv_func.hpp` 内容分歧 → decrypt 改 `dec_aiv_func.hpp`（sync 脚本幂等重放）；合库 AIV-only=5 触 R1 → `kem_dec_g` 改 **MIX 占位**回落 4；main 默认单 session，两段 session 降为 `KEM_DECAPS_SIM_2SESSION=1` 回退。
- **CPU 证据**：`KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `K max=0 PASS`；`out/lib/` 仅 `libascendc_kernels_cpu.so`。

**公司待做（SIM，家里 WSL 不跑重型 SIM）**：

1. **SIM 单 session 验收**：`SIM_DIRECT=1 KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4` → 期望 `K max=0`、`aclrtLaunchKernel 507000` 次数=0、`output/dbg_c_prime.bin == c`（不再 max=244）。
2. **`nm` 审计**：`nm build/CMakeFiles/ascendc_kernels_sim_aiv_device_dir/device_aiv.o | grep funckey` → **AIV-only ≤ 4**（预期 prep_a_hat/prep_re/g4_noise/at_r5）；若出现 507000 说明仍 >4，再挑一个数据通路 AIV 核（如 `decode_t_hat` 已是 MIX，可看 `g4_noise`/`prep_*`）改 MIX 占位。
3. **拒绝路径 SIM**：篡改 `c` 一字节 → `K` 应 `= J(z‖c)`（建议 `KEM_DECAPS_VERIFY=2`）。
4. 扩 `scripts/liboqs_kem_vs_ascendc.sh` **decaps** 段。
5. 单库 SIM 稳定后可删 `KEM_DECAPS_SIM_2SESSION` 回退与 `main_encrypt_g5_run.cpp` 链接。

```bash
# 前置（各跑一次即可；后续 SKIP_REBUILD）
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg20-kem-encaps-k4 && KEM_ENCAPS_SKIP_REBUILD=1 bash run.sh -r cpu -v Ascend910B4

# Decaps CPU 回归（家里已过）
cd ../fix-f203-alg21-kem-decaps-k4
KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
# Decaps SIM 单 session（公司验证；首次务必 FORCE_REBUILD 清旧双库产物）
SIM_DIRECT=1 KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4
```

**WSL 约束**：默认 `CMAKE_BUILD_JOBS=2`；`KERNEL_COMPUTE_BUDGET_SEC=1800`（SIM 长）；勿 `cmake -j` 满核；勿并行多 SIM。

---

### 2. Alg.20 Encaps SIM 复验 + alg14 run.sh 对齐（承接，未完）

| 项 | 状态 |
|----|------|
| **T7a** Alg.20 | **CPU+SIM PASS**（tick ~103 万）；`scripts/liboqs_kem_vs_ascendc.sh` **encaps 段仍待扩** |
| **T7b** alg14 `run.sh` | **待开工**：对齐 alg20（`ENCRYPT_SKIP_REBUILD`/`FORCE_REBUILD`/`CMAKE_BUILD_JOBS=2`；去默认 `rm -rf build out`）；G5 代码已仅输出 `c.bin` |

```bash
cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4
ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4   # 改 run.sh 后回归
SIM_DIRECT=1 ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4
```

---

## ★ 已完成（本推送含）

### Alg.21 Decaps（T7c 有条件 PASS）

| 项 | 内容 |
|----|------|
| **架构** | vendor alg15 Decrypt G4 + alg14 Encrypt G5 + `kem/` K1 `G(m'‖h)` + K2 FO；**已合并单设备库** |
| **I/O** | `dk_kem`(3168)+`c`(1568) → **`K.bin`(32) only**；输入复制 alg19/20（`SEED_D=20260619`） |
| **CPU** | **PASS** `K max=0`（单库单 session + 设备 FO；2026-07-02 合库后回归） |
| **SIM** | **待公司验证**（合库+`kem_dec_g` MIX 后单 session；见 §下一任务 1） |

### Alg.19 KeyGen（T6 PASS） / Alg.20 Encaps（T7a PASS）

| 探针 | 验收 |
|------|------|
| [`fix-f203-alg19-kem-keygen-k4`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/) | CPU+SIM+liboqs **max=0** · SIM **742558** |
| [`fix-f203-alg20-kem-encaps-k4`](ascendc-tests/fix-f203-alg20-kem-encaps-k4/) | CPU+SIM **c/K max=0** · tick **~1029406** |

---

## liboqs / PKE 回归（smoke）

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4   # keygen；encaps/decaps 段待扩

cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg14-pke-encrypt-correctness-k4 && ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg15-pke-decrypt-correctness-k4 && bash run.sh -r cpu -v Ascend910B4
```

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull` |
| 2 | 读本文件 **§下一任务** |
| 3 | [`qa/TODO.md`](qa/TODO.md) · [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| 5 | **禁止**从 `frozen/` 抄码 · **禁止**落盘非算法输出 · **SIM workaround 不是新基线** |

---

## 索引

| 主题 | 链接 |
|------|------|
| 当日纪要 §7 | [`qa/2026-07/2026-07-02-…md`](qa/2026-07/2026-07-02-KEM-Alg19-KeyGen交付与命名纠正.md) |
| Alg.21 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/INTEGRATION_PLAN.md) |
| Alg.21 STATUS | [`fix-f203-alg21-kem-decaps-k4/STATUS.md`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/STATUS.md) |
| Alg.21 note | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
