# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **Cloud / 任意 coding agent 入口**：根 [`AGENTS.md`](AGENTS.md)（硬门禁与文档地图；入口变更时同步刷新）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-14（办公室：`runtime_env` 多环境分流一期落地）

---

## ★ Cloud / 依赖 / 多环境（办公室）

| 项 | 说明 |
|----|------|
| thirdparty | `bash scripts/clone-thirdparty.sh` **默认**编 liboqs 0.15.0 + `liboqs_*_ref`；单独补编：`bash scripts/build-liboqs.sh` |
| Cloud SIM | 若报 `libge_common_base.so … InternalSwap` → **CANN 镜像问题**，非 liboqs；标阻塞；本机 WSL SIM 仍为权威 |
| 多环境分流 | [`scripts/runtime_env.sh`](scripts/runtime_env.sh) · [`docs/engineering/NPU真机环境说明.md`](docs/engineering/NPU真机环境说明.md) |
| `-r` | 默认仍 **cpu**；试点支持 **`auto`** / **`verify`**；WSL **禁 npu** |
| 一期试点 | KEM KeyGen incubating + PKE keygen/encrypt/decrypt 四个 `run.sh`（**未**批量改全仓） |
| 二期 | 活跃 `pass-*` / 其余 exp\|stable 逐步接入；真机再压 npu |

WSL 证据（decrypt）：`verify` cpu+sim PASS；`auto→sim` PASS；`-r npu` 明确失败。KEM：`npu` 拒绝 + `cpu` PASS。

---

## ★ 强制写法（2026-07-08，新代码均须遵守）

| 类型 | 写法 | 禁止 |
|------|------|------|
| CPU vs SIM | `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| SIM host 拓扑 | `ASCENDC_SIM_HOST_MODE` + §3.3 登记表 | `F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等 |
| 算法变体 | CMake 宏（CPU/SIM 同值） | env 切 launch |

详文：[`AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) §4.1 · [`library/shared/INDEX.md`](library/shared/INDEX.md)

**无 NPU 时验收权重**：全链 PKE — **SIM = 主参考**；**CPU = 辅助正确性**。

**Fail 复现纪律**：偶发 FAIL 必须落盘 `mode` + `kem_seed` hex（或 `SEED_D`）+ 错位偏移 + 是否清零 `output/`（`kat_liboqs_kem_keygen.py` 已写 seed；压测脚本同理）。

---

## ★ 当前真相（2026-07-14）

### PKE 三段 — **stable 交付齐备**（未改算法）

| 段 | stable | SIM tick（参考） |
|----|--------|------------------|
| KeyGen | [`stable-fips203-mlkem-pke-keygen-k4`](examples/stable/stable-fips203-mlkem-pke-keygen-k4/) | ~542k |
| Encrypt | [`stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | ~627k |
| Decrypt | [`stable-fips203-mlkem-pke-decrypt-k4`](examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | ~283k |

### KEM Alg.19 KeyGen — **incubating 有条件完成（已入库）**

| 路径 | 角色 |
|------|------|
| [`exp-fips203-mlkem-kem-keygen-k4/`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) | **自包含**实现 + customspec；见 [`STATUS`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/STATUS.md) |
| [`pass-fix-f203-alg19-kem-keygen-device-k4`](ascendc-tests/pass-fix-f203-alg19-kem-keygen-device-k4/) | 行为对照基线（~713k）；**禁止** CMake 依赖本树 |
| `examples/stable/stable-fips203-mlkem-kem-keygen-k4/` | **尚无**；须用户明确 `#交付#` 后从 incubating **复制晋级** |
| registry | [`docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md`](docs/specs/fips203-mlkem1024-kem-keygen-baseline-registry.md) |

**家里验收（已绿）**：CPU×40（清零 output）· SIM tick **707057** · vs correctness×10 · liboqs CPU×10 · **liboqs SIM×3**。

纪要：[`qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md`](qa/2026-07/2026-07-14-KEM-KeyGen-incubating预研重写.md)

---

## ★ 办公室 Agent 下一任务（按优先级）

### P0-1 — 等用户口令后再做：`#交付#` KEM KeyGen → stable

**仅当用户明确说 `#交付#` / `#验收#` 时**：

1. 从 [`exp-fips203-mlkem-kem-keygen-k4`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) **复制晋级**为 `examples/stable/stable-fips203-mlkem-kem-keygen-k4/`。
2. 双模式再验收：`bash run.sh -r cpu` + `SIM_DIRECT=1 bash run.sh -r sim`（或试点 `bash run.sh -r verify`）。
3. 更新 `examples/stable/INDEX.md`、`qa/TODO.md`、当日 `qa/`、本文件。

**未获口令前禁止建 stable。**

### P0-2 — 主线开工：**T19a Encaps device**

[`fix-f203-alg20-kem-encaps-device-k4`](ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/)：改接 stable Encrypt 布局；CPU+SIM；分项 kat。随后 T19b/c Decaps。

### P1 — **T21** SHA3hp · **runtime_env 二期**（其余 `run.sh` 逐步接入）

---

## ★ 家里 / Cloud 注意

- Cloud：`~/ascendc -> /workspace`；先 `clone-thirdparty.sh`。
- 声称用例通过：仍须 **cpu + SIM** 证据；`auto` 不算完整验收。
- WSL：**不要** `-r npu`。
