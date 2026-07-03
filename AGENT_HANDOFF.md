# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-03（**KEM 三分项 kat 全绿**：KeyGen/Encaps/Decaps 均 **CPU+SIM PASS**；Decaps 单库合并后 **SIM 默认 2-session** 可靠，`liboqs` 造密文分项 kat `CPU×10+SIM×1 PASS` · 详当日纪要 [`qa/2026-07/2026-07-03-Alg21-Decaps-SIM单session根因.md`](qa/2026-07/2026-07-03-Alg21-Decaps-SIM单session根因.md)）

---

## ★ 当前真相（KEM 全链，2026-07-03）

三算子设备探针 + liboqs 分项 kat 均通过。**分项 kat** 固定 stash 密钥、每轮换随机量，与 liboqs 逐字节对拍：

| 分项 | 探针 | 分项 kat 脚本 | 随机旁路 | 结果 |
|------|------|---------------|----------|------|
| **KeyGen** Alg.19 | [`fix-f203-alg19-kem-keygen-k4`](ascendc-tests/fix-f203-alg19-kem-keygen-k4/) | `scripts/liboqs_kem_keygen_batch.sh` | `KEM_KG_EXT_SEED=1`（相同 64B `kem_seed=d‖z`） | **CPU×10+SIM×1 PASS** · ek/dk max=0 |
| **Encaps** Alg.20 | [`fix-f203-alg20-kem-encaps-k4`](ascendc-tests/fix-f203-alg20-kem-encaps-k4/) | `scripts/liboqs_kem_encaps_batch.sh` | `KEM_ENC_EXT_SEED=1`（旁路 `m`；`G(m‖H(ek))` 仍 device） | **CPU×10+SIM×1 PASS** · c/K max=0 |
| **Decaps** Alg.21 | [`fix-f203-alg21-kem-decaps-k4`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/) | `scripts/liboqs_kem_decaps_batch.sh` | 无旁路（liboqs `encaps_derand(ek,m)` 每轮造 `c`） | **CPU×10+SIM×1 PASS** · K max=0 |

- **stash**：`output/kem_keypair_stash/{ek_kem,dk_kem}.bin`；`scripts/kem_keypair_stash_bootstrap.sh` 从 alg19 output 复制，keygen kat 首轮 CPU 成功也自动落盘。
- **旁路宏 test-only**：生产默认 `KEM_KG_EXT_SEED=0` / `KEM_ENC_EXT_SEED=0`，零影响；SIM/NPU 宏须 `ascendc_compile_definitions`（`target_compile_definitions` 只作用 CPU → SIM 退 `#else` 全错，教训见 keygen kat）。
- **端到端**（仓库级）：`scripts/liboqs_kem_vs_ascendc.sh` 四阶段逐级对 liboqs、`scripts/roundtrip_kem_keygen_encaps_decaps.sh` 纯 device 闭环，CPU 全绿。

### Decaps SIM 结论（纠正历史“卡死/死锁”误判）

- **不是死锁**：早期把 Phase-E 慢跑（Alg.7 rej 环活跃自旋 ~7min 无输出）误判为 hang；实为慢跑。
- **SIM 默认 2-session 可靠**（`KEM_DECAPS_SIM_2SESSION=1`）：Phase-D `aclFinalize` 后 fresh session 跑 Phase-E + **设备 FO**（无 host memcmp）→ `K max=0`。
- **单 session 首错在 `at_r5`**（`KEM_DECAPS_SIM_2SESSION=0`，排障用）：Phase-D 后 `m'/coins max=0`，但 Phase-E `c' max=244`；PhaseE-only 对照 `K max=0` → 系 **Phase-D 已执行触发的 CAModel session 级状态残留**，非 GM 输入/同步/LUT/算法错误。
- **单库 SIM 构建坑（本轮修）**：`vendor/pke_encrypt/prep/a_hat/alg7/f203_alg7_rej_scalar.c` 是 CPU/参考语义文件，不参与设备热路径；若进 `ascendc_library`，AIC/AIV 合并阶段 `ld.lld -m aicorelinux` 报 `.c.o unknown file type`。修法：CPU twin 编入该 `.c`，SIM/NPU 设备库只保留 `.cpp` kernel 入口 + `.hpp` 内联逻辑（见 `cmake/decaps/CMakeLists.txt`）。

---

## ★ 下一任务（P0/P1）

1. **Alg.21 Decaps 单 session SIM 真修**（对齐 INTEGRATION_PLAN §4.1）：定位 `at_r5` 首错的 CAModel session 残留根因；当前 **2-session 已是可靠保底**，非阻塞交付。
2. **`nm` func_key 审计**：`nm build/CMakeFiles/ascendc_kernels_sim_aiv_device_dir/device_aiv.o | grep funckey` → AIV-only ≤ 4；出现 507000 再挑数据通路 AIV 核改 MIX 占位。
3. **T7b alg14 `run.sh` 资源友好化**：对齐 alg20（`SKIP_REBUILD`/`FORCE_REBUILD`/`CMAKE_BUILD_JOBS=2`；去默认 `rm -rf build out`）。
4. **NPU 实机**：三分项 WSL 均无实机；NPU 未测。
5. 单库 SIM 单 session 稳定后可删 `KEM_DECAPS_SIM_2SESSION` 回退与 `main_encrypt_g5_run.cpp` 链接。

---

## 验收命令（smoke）

```bash
# KEM 分项 kat（默认全量：CPU×10 + SIM×1；前置各跑一次 keygen 或 bootstrap）
bash scripts/liboqs_kem_keygen_batch.sh
bash scripts/kem_keypair_stash_bootstrap.sh
bash scripts/liboqs_kem_encaps_batch.sh
bash scripts/liboqs_kem_decaps_batch.sh

# KEM 端到端 + PKE 回归
bash scripts/liboqs_kem_vs_ascendc.sh -r cpu -v Ascend910B4
bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu -v Ascend910B4
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4

# 单探针（各跑一次即可，后续 SKIP_REBUILD）
cd ascendc-tests/fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg20-kem-encaps-k4 && KEM_ENCAPS_SKIP_REBUILD=1 bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg21-kem-decaps-k4 && KEM_DECAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
```

**WSL 约束**：默认 `CMAKE_BUILD_JOBS=2`；`KERNEL_COMPUTE_BUDGET_SEC` KEM 计算段放宽（Decaps SIM 2-session ~11min/段）；勿 `cmake -j` 满核；勿并行多 SIM。

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull` |
| 2 | 读本文件 **§当前真相 / §下一任务** |
| 3 | [`qa/TODO.md`](qa/TODO.md) · [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 活跃探针 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| 5 | **禁止**从 `frozen/` 抄码 · **禁止**落盘非算法输出 · **旁路宏仅 test-only，非生产基线** |

---

## 索引

| 主题 | 链接 |
|------|------|
| 当日纪要 | [`qa/2026-07/2026-07-03-Alg21-Decaps-SIM单session根因.md`](qa/2026-07/2026-07-03-Alg21-Decaps-SIM单session根因.md) |
| Alg.21 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/INTEGRATION_PLAN.md) |
| Alg.21 STATUS | [`fix-f203-alg21-kem-decaps-k4/STATUS.md`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/STATUS.md) |
| Alg.21 note | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
