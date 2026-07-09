# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-09（**Decrypt 注释 + Alg.15 生产 I/O 收紧**；incubating exp PASS；家里续 **KAT + roundtrip + `#交付#` stable**；Encrypt stable 仍有效）

---

## ★ 强制写法（2026-07-08，新代码均须遵守）

| 类型 | 写法 | 禁止 |
|------|------|------|
| CPU vs SIM | `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| SIM host 拓扑 | `ASCENDC_SIM_HOST_MODE` + §3.3 登记表 | `F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等 |
| 算法变体 | CMake 宏（CPU/SIM 同值） | env 切 launch |

详文：[`AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) §4.1 · [`library/shared/INDEX.md`](library/shared/INDEX.md)

**无 NPU 时验收权重**：Encrypt / Decrypt（及同类全链）— **SIM = 主参考**；**CPU = 辅助正确性**。见 [`F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md`](docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)。

---

## ★ 当前真相（Decrypt 头条 + Encrypt 已交付，2026-07-09）

### Alg.15 Decrypt — **incubating 预研 PASS** ★本轮头条

| 路径 | 角色 |
|------|------|
| [`examples/incubating/exp-fips203-mlkem-pke-decrypt-k4`](examples/incubating/exp-fips203-mlkem-pke-decrypt-k4/) | **预研算子**（待 `#交付#`） |
| [`ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4`](ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/) | **PASS 探针**（roundtrip/liboqs 默认 Decrypt） |
| customspec | [`…-实现方案-customspec.pdf`](examples/incubating/exp-fips203-mlkem-pke-decrypt-k4/exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.pdf) |

| 项 | 内容 |
|----|------|
| I/O（Alg.15） | **生产** `input/`：**仅** `dk_pke`+`c`+`lut_*` → **out 仅 `m` 32B**；造 c 夹具在 `output/_gen_fixture/`（**不进** input） |
| Launch | **1×** `f203_decrypt_device_fused`（MIX `aicore=1`；GATE 4/8 + softSync） |
| SIM（主参考） | `m` max=0；tick **~283k**（exp **283290** / 探针 **283252**） |
| CPU（辅助） | `m` max=0 |
| 本轮已做 | 主路径**详细中文注释**（exp↔pass-fix 同步）；`gen_data` scrub 非生产 input |
| **家里必做** | ① Decrypt **KAT**（仿 Encrypt `kat_liboqs_vs_ascendc.sh`）② **roundtrip**（`DECRYPT_DIR`→exp）③ **`#交付#`** → `stable-fips203-mlkem-pke-decrypt-k4`（T15a） |

纪要：[`qa/2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md`](qa/2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md) §10.8–§10.10

### Alg.14 Encrypt — **stable 交付**（已完成，勿重做）

算子：[`examples/stable/stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)  
I/O：`ek_pke`+`m`+`coins` → **仅 `c`**；SIM tick ~627k；KAT×10+1 + roundtrip×10+1 PASS。

### 仓库闭环脚本默认

| 段 | 默认 |
|----|------|
| KeyGen | `examples/stable/stable-fips203-mlkem-pke-keygen-k4` |
| Encrypt | `examples/stable/stable-fips203-mlkem-pke-encrypt-k4` |
| Decrypt | `ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4`（晋级后改 stable） |

---

## ★ 下一任务（P0）— 家里 Agent

1. **Decrypt KAT**：在 `exp-fips203-mlkem-pke-decrypt-k4` 补 `kat_liboqs_vs_ascendc.sh`（或等价），CPU×10 + SIM×1 vs liboqs。
2. **round-trip 批测**：`DECRYPT_DIR=…/exp-fips203-mlkem-pke-decrypt-k4 bash scripts/roundtrip_pke_batch.sh`（确认 m max=0）。
3. **`#交付#`**：复制晋级 → `examples/stable/stable-fips203-mlkem-pke-decrypt-k4`；刷新 INDEX / registry / roundtrip 默认 `DECRYPT_DIR`。
4. （次优）Alg.21 Decaps 单 session SIM；NPU 实机；T7b correctness `run.sh`。

**禁止**：从 `frozen/` 抄码；擅自改已锁定 I/O/tiling；把夹具 ek/m/coins 再写回生产 `input/`。

---

## 验收命令（smoke）

```bash
# ★ Decrypt incubating（本轮主线）
cd examples/incubating/exp-fips203-mlkem-pke-decrypt-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # m max=0；tick ~283k；input/ 仅 dk+c+lut_*

# PASS 探针（对照 / 当前 roundtrip 默认）
cd ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

# 闭环（Decrypt 仍默认 pass-fix；测 exp 时覆盖 DECRYPT_DIR）
bash scripts/roundtrip_pke_batch.sh
# DECRYPT_DIR=$PWD/examples/incubating/exp-fips203-mlkem-pke-decrypt-k4 bash scripts/roundtrip_pke_batch.sh

# Encrypt stable（回归）
cd examples/stable/stable-fips203-mlkem-pke-encrypt-k4
bash run.sh -r sim -v Ascend910B4
```

**WSL**：`CMAKE_BUILD_JOBS=2`；勿并行多 SIM；默认 sim 已 `SIM_DIRECT=1`。

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull` |
| 2 | 读本文件 **§当前真相 / §下一任务** |
| 3 | 当日纪要 [`qa/2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md`](qa/2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md) §10 |
| 4 | exp [`STATUS.md`](examples/incubating/exp-fips203-mlkem-pke-decrypt-k4/STATUS.md) + customspec PDF |
| 5 | [`qa/TODO.md`](qa/TODO.md) · Rule `ascendc-development.mdc` |
| 6 | **禁止** frozen 抄码 · 生产 input **仅** dk+c+lut |

### 接手步骤（Decrypt `#交付#`）

1. 确认 `input/` 无 ek/m/coins/meta；`gen_data` 夹具在 `_gen_fixture/`。
2. 跑上方 Decrypt smoke（CPU+SIM）；再 KAT + roundtrip。
3. 用户确认 `#交付#` 后：从 **活跃 exp** 复制晋级 stable（**非** frozen）；改 roundtrip 默认 `DECRYPT_DIR`。

---

## 附录：关键路径速查

| 主题 | 路径 |
|------|------|
| **Decrypt incubating** | [`examples/incubating/exp-fips203-mlkem-pke-decrypt-k4/`](examples/incubating/exp-fips203-mlkem-pke-decrypt-k4/) |
| **Decrypt PASS 探针** | [`ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/`](ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/) |
| **Encrypt stable** | [`examples/stable/stable-fips203-mlkem-pke-encrypt-k4/`](examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) |
| 2-launch Decrypt 编排笔记 | [`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md)（历史；生产已是 1-kernel） |
| 闭环脚本 | [`scripts/roundtrip_pke_batch.sh`](scripts/roundtrip_pke_batch.sh) |
| 探针索引 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| 内核超时 | [`docs/engineering/内核计算超时与性能定标.md`](docs/engineering/内核计算超时与性能定标.md) |
