# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-06（**Alg.14 Encrypt compute 行 18–19**：3 launch CPU+SIM + 单 launch SIM 全绿；û **UB 驻留**定案 · 详 [`qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md`](qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md)）

---

## ★ 当前真相（Encrypt compute + KEM，2026-07-06）

### Alg.14 Encrypt compute（**今日主战场**）

探针：[`fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/)

| 模式 | 环境 | 结果 |
|------|------|------|
| 3 launch（`ntt_y` \| `at_jp` \| `intt_e1`） | CPU + SIM | `y_hat` / `u_ntt` / `u` **max=0** |
| 单 launch（`f203_encrypt_l18_l19`，`F203_FEAS_FUSED=1`） | SIM only | 同上 **max=0**（~130s） |
| 单 launch | CPU | **不支持**（tikicpu MIX 串行死锁） |

**定案**：内积 `û` **驻留 UB** → `AivK8Split::ProcessFromLocal`；禁止同 kernel 标量写 GM + MTE `DataCopy` 读 GM（SIM `u≈e₁` 根因）。MIX 握手：**ST_IP_AIV_DONE=4** → **ST_AT_JP_GATE=8**；INTT CrossCore **1/3**。

**未做**：kP=5（`tr̂`）、行 21 `v`、行 2 `t̂` decode、prep+compute 2 launch 全链。

### KEM 三分项（维持 07-03 结论）

KeyGen / Encaps / Decaps 分项 kat **CPU×10+SIM×1 PASS**；Decaps SIM 默认 **2-session** 可靠；单 session `at_r5` 首错仍待真修（非阻塞）。

---

## ★ 下一任务（P0）

1. **T17 Encrypt compute 扩展**（[`INTEGRATION_PLAN.md` §8.4](ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/INTEGRATION_PLAN.md)）：
   - at_y5 **kP=5** + INTT batch（k=5 或 pad→8）+ **e₂** → `v`
   - 行 2 **`decode_t_hat`** + 设备拼 `matM`
   - 与 [`fix-f203-alg14-lines3-15-encrypt-prep-k4`](ascendc-tests/fix-f203-alg14-lines3-15-encrypt-prep-k4/) **拼接** → 目标 Encrypt 核心 **2 launch**
2. **Alg.21 Decaps 单 session SIM 真修**（2-session 已是保底）。
3. **T7b alg14 correctness `run.sh` 资源友好化**（对齐 alg20）。
4. **NPU 实机**：KEM + PKE 均未测。

---

## 验收命令（smoke）

```bash
# Encrypt compute（今日）
cd ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
F203_FEAS_FUSED=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # 单 launch，~130s

# Encrypt prep（前置）
cd ../fix-f203-alg14-lines3-15-encrypt-prep-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# KEM 分项 kat（回归）
bash scripts/liboqs_kem_keygen_batch.sh
bash scripts/kem_keypair_stash_bootstrap.sh
bash scripts/liboqs_kem_encaps_batch.sh
bash scripts/liboqs_kem_decaps_batch.sh
```

**WSL 约束**：`CMAKE_BUILD_JOBS=2`；compute 单 launch 默认 `KERNEL_COMPUTE_BUDGET_SEC=180`；勿并行多 SIM。

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull` |
| 2 | 读本文件 **§当前真相 / §下一任务** |
| 3 | 当日纪要 [`qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md`](qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md) |
| 4 | 定稿 [`docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md) — **写融合码前必读** |
| 5 | [`qa/TODO.md`](qa/TODO.md) T17 · [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 6 | **禁止**从 `frozen/` 抄码 · **中间态驻 UB**，GM 仅 dump 对拍 |

### 接手步骤（Encrypt compute 延续）

1. 读探针 [`STATUS.md`](ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/STATUS.md) + [`INTEGRATION_PLAN.md` §8](ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/INTEGRATION_PLAN.md)。
2. 跑上面三条验收命令确认基线仍绿。
3. 下一刀建议：**kP=5 内积**（扩 `innerproduct_halfrows_to_ub` 与 golden）→ 再并 INTT 第 5 行 + e₂。
4. 单 launch 扩 v 前先算 UB 预算；不够则维持 3 launch 或退 2 launch，**勿**为绕过编译擅自改已锁定形状。
5. 写 AscendC 前查 [`CANN-AscendC算子开发接口参考-查阅索引.md`](library/documents/CANN-AscendC算子开发接口参考-查阅索引.md)。

---

## 索引

| 主题 | 链接 |
|------|------|
| 当日纪要 | [`qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md`](qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md) |
| compute 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/INTEGRATION_PLAN.md) |
| compute STATUS | [`STATUS.md`](ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/STATUS.md) |
| UB 驻留 note | [`docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md) |
| 上游 2s1e note | [`docs/notes/F203-2s1e-NTT内积UB融合技术总结.md`](docs/notes/F203-2s1e-NTT内积UB融合技术总结.md) |
| prep 探针 | [`fix-f203-alg14-lines3-15-encrypt-prep-k4`](ascendc-tests/fix-f203-alg14-lines3-15-encrypt-prep-k4/) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
| KEM 07-03 纪要 | [`qa/2026-07/2026-07-03-Alg21-Decaps-SIM单session根因.md`](qa/2026-07/2026-07-03-Alg21-Decaps-SIM单session根因.md) |
