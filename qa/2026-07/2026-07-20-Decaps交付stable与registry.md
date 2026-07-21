# 2026-07-20 — Decaps `#交付#` stable · registry · 六算子齐

## 结论

[`exp-fips203-mlkem-kem-decaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/) 按 customspec 落地自包含实现：**CPU+SIM 全链绿**。

| 项 | 证据 |
|----|------|
| I/O | `dk_kem`+`c`+LUT → 仅 `K`；`K` max=0 |
| Launch | SIM 4 / CPU 6；单库；`decaps_1session` |
| tick | D **286999** + E **745790**（对标 pass-fix） |
| vendor | `decrypt/` + `prep/`/`compute/` + `kem/`；`prepare_dec_shim.sh` |

## 验收（预研阶段；同日上午）

```bash
cd examples/incubating/exp-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## Round-trip（同日续；中间态 → 终态）

| 阶段 | `DECAPS_DIR` 默认 | 证据 |
|------|-------------------|------|
| 预研绿后 | incubating `exp-…-kem-decaps-k4` | roundtrip CPU+SIM agreement+reject **PASS** |
| `#交付#` 后 | **stable** `stable-…-kem-decaps-k4` | 同脚本复验 **PASS**（见下「复验证据」） |

涉及脚本：`roundtrip_kem_decaps.sh` / `roundtrip_kem_keygen_encaps_decaps.sh` / `liboqs_kem_vs_ascendc.sh` / `kat_liboqs_kem_decaps.py`。

## 下一刀（交付后仍开）

- ✅ `#交付#` → stable（见下节）
- **T19i**：`fo_only` 内联（SIM 4→3）；**T2-npu**：NPU 实机

## 登记表

已刷新 [`qa/active_sim_regress_summary.md`](../active_sim_regress_summary.md)：stable Decaps **1032762**；incubating 同期 D**286999**+E**745790**；`scripts/` Decaps 默认终态 → **stable**。

## liboqs 分项 KAT（同日续）

`KEM_DEC_CPU_TRIALS=10 KEM_DEC_SIM_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh`（当时 `DECAPS_DIR`→exp；交付后默认同脚本指 stable 复验绿）：

| 模式 | 结果 |
|------|------|
| CPU×10 | **PASS** |
| SIM×3 | **PASS** |

固定 stash `dk`；每轮 liboqs encaps→`c`；device `K` 逐字节一致。

## baseline-registry（同日续）

新增 [`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md)：登记合法/拒绝路径 golden 块（liboqs encaps/decaps、`J`/`G` Host 对照、LUT、CPU `golden_v`）；晋级后「适用」已改指 stable。

## Skill + 历史缺表补登记（同日续）

约定：**registry 硬卡点放在 Skill，不改 Rule** — `#交付#`/`#验收#` 晋级前必须定稿；`$规格$`/【预研】允许缺表。

| 变更 | 路径 |
|------|------|
| Skill | [`ascendc-delivery`](../../.cursor/skills/ascendc-delivery/SKILL.md) 门禁§3 + 清单；[`pre-research`](../../.cursor/skills/pre-research/SKILL.md)「收敛 toward delivery」提示 |
| 补登记 | [`pke-keygen-baseline-registry`](../../docs/specs/fips203-mlkem1024-pke-keygen-baseline-registry.md)、[`pke-decrypt-baseline-registry`](../../docs/specs/fips203-mlkem1024-pke-decrypt-baseline-registry.md) |
| 索引 / STATUS | `docs/specs/INDEX.md`；stable PKE KeyGen / Decrypt `STATUS.md` 链到 registry |

当前六表齐：PKE KeyGen / Encrypt / Decrypt · KEM KeyGen / Encaps / Decaps。

## `#交付#` Decaps → stable（同日续）

| 项 | 内容 |
|----|------|
| 晋级 | 复制 [`exp-…-kem-decaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/) → [`stable-…-kem-decaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/) |
| 默认路径 | 仓库 `DECAPS_DIR` → stable；registry「适用」→ stable |
| 复验 | stable CPU / SIM / KAT×10+3 / roundtrip（含拒绝）— 见 STATUS |


## `#交付#` stable 复验证据（同日续）

目录：[`stable-fips203-mlkem-kem-decaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/)

| 项 | 结果 |
|----|------|
| `run.sh` CPU | **PASS**（`K` max=0） |
| `run.sh` SIM | **PASS** tick **1032762**=D**286896**+E**745866**；无 stray dump |
| liboqs KAT | **PASS** CPU×10 + SIM×3（CPU 曾一轮 flake，复测绿） |
| roundtrip | **PASS** CPU+SIM（agreement + reject） |

`DECAPS_DIR` 默认已指向 stable。


## 收尾：纪要刷新 · 备份 · 合入 main（同日续）

| 项 | 说明 |
|----|------|
| 纪要 / 索引 | 本日文件更名关键词；`qa/INDEX` · `qa/2026-07/INDEX` · `qa/TODO` · `AGENT_HANDOFF` · `AGENTS` · `README` · 各 `examples/*/INDEX` |
| 近期核对 | 07-15 Encaps `#验收#`、07-17 T2、07-18 pass-fix/`$规格$` 已有日纪要；本文件补齐当日【预研】→registry→`#交付#`全链；07-18「未提交」已标注闭合 |
| 本地备份 | `bash backup-project.sh` → **`backup/v0.1_20260720053001`**；收尾再刷一版见下 |
| 合入 | feature `cursor/stable-kem-decaps-delivery-8244` → **`main`**（`42d4f24`）并推送 |
| 里程碑 | PKE 三段 + KEM KeyGen/Encaps/**Decaps** **六算子 stable 齐** |

## 再收尾：缺口补记 · 二次备份 · 推 main（同日续）

| 项 | 说明 |
|----|------|
| 补记 | `qa/TODO` 正式挂 **T19i**；07-18 纪要「未提交」→ 已合入说明；本文件验收/DECAPS 路径按时间线澄清 |
| 二次备份 | `bash backup-project.sh` → **`backup/v0.1_20260720053530`**（4035 文件） |
| 推送 | 文档补丁直接落 **`main`** |


## 关闭 T13b / T11（同日续；已取代）

用户确认：两实验目标已被现有 stable KeyGen 覆盖，**不再单独维护 TODO**。

| ID | 原目标 | 关闭理由 |
|----|--------|----------|
| **T13b** | fork `vec-k4-v2`→`vec-k4-v3`（V3 预采样 + 设备 `a_hat`） | [`stable-…-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/) / KEM KeyGen 已是 Launch1 prep（设备 Â + V3 行 8–15）+ Launch2 2s1e；不必再做独立 v3 探针包装 |
| **T11** | 2s1e 探针/exp → 单独 `examples/stable/` | 2s1e 已随 KeyGen `compute/` 定型交付；不另开裸 2s1e stable 算子 |

**仍保留**：探针 [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) 作 compute 对照（host 喂 `a_hat` / `KEYGEN_ORCHESTRATE`）；**不**再排 T13b/T11 晋级。

打开项主线：**T23** · **T2-npu** · **T21**（**T19i 已关**）。


## 挂账 T23：多 AI Core 并行 stable（同日续）

用户指定下一实验：

| 项 | 内容 |
|----|------|
| **ID** | **T23**（P1） |
| **首刀** | **2 颗 AI Core**，每 Core 跑一份 **stable** 算子（乃至一轮 **round-trip**） |
| **理论** | **N 颗 AI Core ≈ N 路并行 stable**（实例级并行；≠ 单算子内双 AIV 分片） |
| **状态** | 已写入 [`qa/TODO.md`](../TODO.md) §打开项 + §T23；**待开工** |
| **非目标** | 不改现有 stable 默认单实例；见 TODO 验收草案 |


## 本地备份 · 合入 main（同日续）

| 项 | 说明 |
|----|------|
| 备份 | `bash backup-project.sh` → **`backup/v0.1_20260720080557`**（4072 文件） |
| 范围 | 关闭 T13b/T11 + 挂账 T23 + 当日纪要 / INDEX / HANDOFF |
| 合入 | feature `cursor/close-t13b-t11-superseded-8244` → **`main`** 并推送 |


## Decaps 更名幽灵目录 · 引用清理（同日续）

| 项 | 说明 |
|----|------|
| **成因** | 2026-07-18 `git mv` `fix-…-decaps-device`→`pass-fix-…` **只搬跟踪文件**；旧路径留下 `build_*`/`input`/`output` 等未跟踪空壳 |
| **会否再生** | **不会**被 `run.sh`/scripts 自动创建（默认已指 stable / pass-fix）；仅当有人 `mkdir` 旧名、或照错误文档链 `pass-probe-*` 误建 |
| **已做** | 删空壳；全文纠 `pass-probe-…`→`pass-fix-…`（HANDOFF/AGENTS/README/registry/STATUS/INDEX 等）；[`ascendc-tests/INDEX.md`](../../ascendc-tests/INDEX.md) 加「更名防幽灵」；新增 [`scripts/cleanup-ascendc-test-ghosts.sh`](../../scripts/cleanup-ascendc-test-ghosts.sh) |
| **禁名** | `fix-f203-alg21-kem-decaps-device-k4`（已更名）；`pass-probe-*`（误名，从未权威） |
| **仍保留** | correctness 已迁入 `frozen/`（见下节） |

## 冻结 KEM Alg.19/20/21 correctness 三探针（同日续）

| 原活跃路径 | 冻结后 |
|------------|--------|
| `fix-f203-alg19-kem-keygen-correctness-k4` | [`frozen-fix-…-alg19-…-correctness-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg19-kem-keygen-correctness-k4/) |
| `fix-f203-alg20-kem-encaps-correctness-k4` | [`frozen-fix-…-alg20-…-correctness-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg20-kem-encaps-correctness-k4/) |
| `fix-f203-alg21-kem-decaps-correctness-k4` | [`frozen-fix-…-alg21-…-correctness-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg21-kem-decaps-correctness-k4/) |

| 项 | 说明 |
|----|------|
| 原因 | 正确性路标完成；继任 **stable + pass-fix device** |
| 关闭 TODO | **T6 / T7a / T7c** |
| 合入 | **main**（2026-07-20；**不另起分支**） |

## stable KEM ↔ liboqs 一键回归（同日续；随机字节 · CPU+SIM 全绿）

| 项 | 说明 |
|----|------|
| **入口** | [`scripts/stable_kem_liboqs_roundtrip.sh`](../../scripts/stable_kem_liboqs_roundtrip.sh) |
| **语义** | **先** `liboqs_kem_fixture.py --random`（`urandom` 64B `kem_seed=d‖z` + 32B `m` → liboqs derand 出向量），**再**把**同一批字节**喂 AscendC |
| **AscendC 接线** | KeyGen：`KEM_KG_EXT_SEED=1` + `kem_seed.bin`；Encaps：`M_FILE=m.bin`；Decaps：同次 KeyGen 的 **`EK_KEM_SRC`+`DK_KEM_SRC`**（缺 ek 会回落 stash → FO 假拒） |
| **默认** | 同一 fixture 下 **CPU×1 + `SIM_DIRECT` sim×1**；二者都绿才算验收；定点复现可 `KEM_SEED_HEX`/`M_HEX` |
| **非本脚本** | `liboqs_kem_vs_ascendc.sh` 仍是定点 `SEED_D` derand（生产自派生对照） |
| **SIM 清理** | 默认清 Decaps `build_prod_sim`（`kem/`→`compute/` l18 幽灵 `.o` → `multiple definition`） |

### 复验证据（2026-07-20）

```bash
bash scripts/stable_kem_liboqs_roundtrip.sh
# → [SUCCESS] … fixture=output/stable_kem_liboqs_rt/20260720_092533_195335/
```

| 模式 | KeyGen | Encaps | Decaps accept | Decaps reject |
|------|--------|--------|---------------|---------------|
| CPU | max=0 | max=0 | max=0 + agreement | max=0 |
| SIM | max=0 | max=0 | max=0 + agreement | max=0 |

Decaps SIM tick（本轮）：D **286851** + E **746275**（对标 T2 基线 D≈286803 / E≈745925）。

### 踩坑（同日）

| 现象 | 根因 | 修法 |
|------|------|------|
| Encaps vs fixture 假红（定点路径） | stable Encaps 默认定点 `m=0`，fixture SHA3 派生 `m` | Phase 2 喂 `M_FILE=fixture/m.bin`；随机路径本就用同字节 |
| Decaps `K` max≠0、Encaps 却绿 | gen_data 未设 `EK_KEM_SRC` → 读旧 `kem_keypair_stash` ek，与本次 `dk` 不一致 → FO 拒 | roundtrip / `liboqs_kem_vs` / `roundtrip_kem_*` 同步传 `EK_KEM_SRC` |
| Decaps SIM `multiple definition` `f203_encrypt_l18_l19` | 源已迁 `compute/`，`build_prod_sim` 残留 `kem/` 幽灵 `.o` | 脚本默认 `rm -rf build_prod_sim`；同类幽灵仅 Decaps 族（stable/exp/pass-fix） |

## T19i pass-probe/`pass-fix`：`fo_only`→`l18_l19`（同日续）

目录：[`pass-fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)（用户口中的 pass-probe 已更名）。

| 项 | 内容 |
|----|------|
| 方案 | `INTEGRATION_PLAN` §7：探针本地覆盖 `l18_l19`；禁止改共享 Encrypt |
| 实现 | pack 后 `SyncAll<isAIVOnly>`；AIV0 `KemDecFo`；Host 删 `fo_only` |
| CPU | **PASS** |
| SIM | **PASS** D**287037**+E**763886**；3 launch |
| 拒绝 | CPU+SIM **PASS** |
| 未做 | stable/exp Decaps 镜像（仍 SIM 4） |


## T19i incubating customspec 修订（同日续；仅规格）

路径：[`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex/.pdf`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/)

| 项 | 内容 |
|----|------|
| 锁定 | 生产 SIM **3** launch；FO 并入 `l18_l19` 尾（`SyncAll` + AIV0 `KemDecFo`） |
| CPU | 仍 6（`pack_fo`） |
| 禁止 | 默认 `fo_only`；回写共享 PKE Encrypt `compute/` |
| 基线 tick | pass-fix T19i D**287037**+E**763886** |
| 未做 | 本目录【迭代】写码；stable customspec/实现 |

本轮按 ascendc-impl-spec **只改规格**；待用户确认并说「可以写代码」/【迭代】后再改实现。


## T19i incubating【迭代】落地（同日续）

目录：[`exp-fips203-mlkem-kem-decaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/)

| 项 | 结果 |
|----|------|
| 规格 | 已确认 customspec SIM 3 |
| 实现 | `kem/f203_encrypt_l18_l19_kernel.cpp` 覆盖；Host 删 `fo_only` |
| CPU | **PASS** |
| SIM | **PASS** D**286846**+E**763935** |
| 拒绝 | CPU+SIM **PASS** |
| 未做 | stable Decaps 镜像 |


## T19i stable `#修改#`（同日续）

目录：[`stable-fips203-mlkem-kem-decaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/)

| 项 | 结果 |
|----|------|
| customspec | SIM **3** 锁定；验收含 KAT + roundtrip |
| 实现 | 镜像 exp：`kem/l18_l19` 覆盖；Host 删 `fo_only` |
| CPU / SIM | **PASS**；D**286851**+E**763769** |
| 拒绝 | CPU+SIM **PASS** |
| liboqs KAT | **PASS** CPU×10 + SIM×3 |
| roundtrip | **PASS** cpu+sim（agreement + reject） |
| T19i | **关闭**（pass-fix + exp + stable 齐） |


## WSL：`stable_kem_liboqs_roundtrip` 连续 SIM 偶发（同日续）

来源：办公室 **WSL Agent** 跑 [`scripts/stable_kem_liboqs_roundtrip.sh`](../../scripts/stable_kem_liboqs_roundtrip.sh) 反馈（fixture `output/stable_kem_liboqs_rt/20260720_185052_42894/`）。

| 段 | 结果 |
|----|------|
| Phase 0 liboqs fixture | OK |
| CPU KeyGen / Encaps / Decaps accept / reject | 全 **PASS**（max=0） |
| SIM KeyGen | **PASS**（tick **700879**） |
| SIM Encaps | **PASS**（tick **719417**） |
| SIM Decaps accept | **失败**：Phase-D ~130s 后 `tcache_thread_shutdown(): unaligned tcache chunk detected` → core dump / Aborted |
| 同 fixture **单独**重跑 Decaps SIM | **PASS**（D**286698**+E**754823**；`K` max=0） |

| 解读 | 说明 |
|------|------|
| 正确性 | CPU 全绿 + 同 fixture 单独 Decaps SIM 绿 → **非**稳定算法/接线错 |
| 失败形态 | **连续 SIM**（KeyGen→Encaps→立刻 Decaps）时 CAModel/堆偶发；Cloud 同日端到端亦曾一次全绿（fixture `20260720_102426_216972`） |
| `tee` 陷阱 | 脚本内已有 `set -euo pipefail`；外层 `bash script \| tee log` **未** `set -o pipefail` 时，外层可能仍 exit 0，**掩盖**脚本失败 |

推荐调用：

```bash
set -o pipefail
bash scripts/stable_kem_liboqs_roundtrip.sh 2>&1 | tee /tmp/stable_kem_rt.log
echo EXIT:$?
```

若端到端 SIM Decaps 再遇 `tcache`：用同 fixture 对 Decaps 目录单独 `run.sh -r sim` 复验；绿则记环境偶发，勿当算法回归。

> **2026-07-21 对照矩阵**：见 [2026-07-21-连续SIM-tcache对照矩阵.md](2026-07-21-连续SIM-tcache对照矩阵.md) — C0–C5 各×3，**18/18 绿、tcache 0**；未改脚本。


## 性能登记刷新（同日续；T22）

刷新 [`qa/active_sim_regress_summary.md`](../active_sim_regress_summary.md) + stable Decaps `STATUS`：

| 来源 | Decaps SIM tick |
|------|-----------------|
| **登记（Cloud 端到端全绿）** | **1041906**（D**286865**+E**755041**；fixture `20260720_102426_216972`） |
| T19i 单算子验收（保留对照） | 1050620（D286851+E763769） |
| WSL 单独 Decaps（同脚本偶发后复验） | 1041521（D286698+E754823；fixture `20260720_185052_42894`） |

KeyGen/Encaps 主登记仍用交付 STATUS；备注补 roundtrip 复测 tick（Cloud/WSL）。清除 incubating「stable 仍 4」过时备注。
