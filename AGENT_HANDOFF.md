# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。  
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。  
> **最后刷新**：2026-07-01（**KEM Alg.16 KeyGen 探针规划** · 详 [`qa/2026-07/2026-07-01-liboqs验证与KEM-Alg16-KeyGen规划.md`](qa/2026-07/2026-07-01-liboqs验证与KEM-Alg16-KeyGen规划.md) §7）

---

## ★ 下一任务（家里 Agent · P0）

### ML-KEM Alg.16 KeyGen 正确性探针（**代码未写**）

| 项 | 内容 |
|----|------|
| **目录** | [`ascendc-tests/fix-f203-alg16-kem-keygen-k4/`](ascendc-tests/fix-f203-alg16-kem-keygen-k4/) |
| **方案** | [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg16-kem-keygen-k4/INTEGRATION_PLAN.md) · [`SELF_CONTAINED.md`](ascendc-tests/fix-f203-alg16-kem-keygen-k4/SELF_CONTAINED.md) |
| **原理** | [`docs/notes/F203-KEM-Alg16-KeyGen设备全链技术总结.md`](docs/notes/F203-KEM-Alg16-KeyGen设备全链技术总结.md) |
| **TODO** | **T6**（P0 进行中） |

**用户锁定**：

- **ml_kem_1024（k=4）**；`ek_kem` **1568B** · `dk_kem` **3168B**（liboqs：`dk_pke‖ek‖H(ek)‖z`）
- KEM 增量（`H(ek)`、采 `z`、拼接）**全在 device**；SHA3 用 `library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`
- PKE 段 **vendor 自** [`stable-mlkem-f203-pke-keygen-k4`](examples/stable/stable-mlkem-f203-pke-keygen-k4/)，**禁止** `#include` stable 路径、**禁止**子进程调 stable `run.sh`
- **不做** `examples/exp-*`；Encaps/Decaps 后续独立探针

**建议实现顺序**（INTEGRATION_PLAN §8）：

1. `vendor_sync` + `run.sh`/`CMakeLists` 壳（G0）
2. vendor PKE G1 vs stable
3. 设备 `H(ek)` G2
4. 锁定 `z` 采样（先 liboqs fixture dump）
5. G3 拼接 + CPU+SIM `KEM_KEYGEN_VERIFY=1`
6. 仓库 `scripts/liboqs_kem_vs_ascendc.sh`（待建）L2 对拍

---

## ★ 验收声明（PKE · 已完成）

### liboqs PKE 三阶段交叉验证

**结论**：KeyGen / Encrypt / Decrypt 分别与 liboqs **字节对拍 max=0**（`SEED_D=20260619`）。

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/liboqs_pke_vs_ascendc.sh -r sim -v Ascend910B4
```

详 [`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md) · **`Compress_5` `(1<<26)`** 修复。

### PKE 探针 + round-trip

| 算子 | 路径 | 验收 |
|------|------|------|
| Encrypt | [`fix-f203-alg14-pke-encrypt-correctness-k4`](ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/) | CPU+SIM c max=0 · SIM **922441** tick |
| Decrypt | [`fix-f203-alg15-pke-decrypt-correctness-k4`](ascendc-tests/fix-f203-alg15-pke-decrypt-correctness-k4/) | CPU+SIM m max=0 · **2 launch** |
| KeyGen stable | [`stable-mlkem-f203-pke-keygen-k4`](examples/stable/stable-mlkem-f203-pke-keygen-k4/) | liboqs ek/dk max=0 |
| Round-trip | [`scripts/roundtrip_pke_encrypt_decrypt.sh`](scripts/roundtrip_pke_encrypt_decrypt.sh) | device 闭环 CPU+SIM |

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull`（**主代码源**是 GitHub，不是 `backup/`） |
| 2 | 读本文件 **§下一任务** |
| 3 | [`README.md`](README.md) → [`qa/TODO.md`](qa/TODO.md) → [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 4 | 读探针 [`INTEGRATION_PLAN.md`](ascendc-tests/fix-f203-alg16-kem-keygen-k4/INTEGRATION_PLAN.md) 再写码 |
| 5 | 活跃探针以 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) 为准；**禁止**从 `frozen/` 抄码 |

---

## 1. 运行约定（默认，勿手动 export）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

kernel 计算默认 **≤15s**（`KERNEL_COMPUTE_BUDGET_SEC`）。

---

## 2. GitHub 同步

**推荐**：`git pull origin main`

### `thirdparty/` — 本地依赖（**不进 GitHub**）

| 目录 | 用途 |
|------|------|
| `liboqs/` | PKE/KEM liboqs 交叉验证 |
| `tiny_sha3/` | Host golden **仅** VERIFY 路径 |

**liboqs 前**：`bash scripts/build_liboqs_pke_ref.sh`

---

## 3. smoke（拉代码后 · PKE 回归）

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4

cd ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg15-pke-decrypt-correctness-k4 && bash run.sh -r cpu -v Ascend910B4
bash ../../scripts/roundtrip_pke_encrypt_decrypt.sh -r cpu -v Ascend910B4
```

KEM 探针实现后追加本段 smoke。

---

## 4. 索引

| 主题 | 链接 |
|------|------|
| 当日纪要 | [`qa/2026-07/2026-07-01-liboqs验证与KEM-Alg16-KeyGen规划.md`](qa/2026-07/2026-07-01-liboqs验证与KEM-Alg16-KeyGen规划.md) |
| KEM Alg.16 note | [`docs/notes/F203-KEM-Alg16-KeyGen设备全链技术总结.md`](docs/notes/F203-KEM-Alg16-KeyGen设备全链技术总结.md) |
| KEM 探针 STATUS | [`fix-f203-alg16-kem-keygen-k4/STATUS.md`](ascendc-tests/fix-f203-alg16-kem-keygen-k4/STATUS.md) |
| 活跃探针 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| 遗留总表 | [`qa/TODO.md`](qa/TODO.md) |
