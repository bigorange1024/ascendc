# 2026-07-09 — Encrypt 探针默认 `SIM_DIRECT` + 工程债 1–3 + 路线 11 关闭

## 1. 用户约束：默认即最优路径，勿再要求手动编译/运行选项

用户明确：测试阶段可用编译/运行选项切代码段；**代码稳定后**须改成默认跑最优最正确路径，**不要让用户再手动输入更多选项**（含 `SIM_DIRECT=1`）。

说明：`SIM_DIRECT` 本身不是编译宏，而是 sim 是否走 msprof/`OPPROF_*` 的运行开关。成熟探针（keygen/encaps）早已在 `run.sh` 的 sim 分支内 `export SIM_DIRECT=1`；Encrypt 系列文档却仍写 `SIM_DIRECT=1 bash run.sh …`，违反「默认 = 生产全量」口径。

## 2. 已改（2026-07-09）— 默认 `SIM_DIRECT=1`

下列 PASS 探针 `run.sh` 在 `RUN_MODE=sim` 时自动 `export SIM_DIRECT="${SIM_DIRECT:-1}"`；Usage 注释改为：

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # 无需手动 SIM_DIRECT
```

| 探针 | 改动 |
|------|------|
| `pass-fix-f203-alg14-pke-encrypt-device-k4` | run.sh + STATUS + INTEGRATION_PLAN |
| `pass-fix-f203-alg14-lines3-15-encrypt-prep-k4` | run.sh |
| `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | run.sh |
| `pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4` | run.sh |
| `pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4` | run.sh |

`AGENT_HANDOFF.md` smoke 命令同步去掉手动 `SIM_DIRECT=1`。调试采性能仍可显式 `SIM_DIRECT=0`（非默认）。

## 3. 全链 Encrypt 现状（承接 07-08）

- 探针：`pass-fix-f203-alg14-pke-encrypt-device-k4`
- I/O：in `ek+m+coins` → **out 仅密文 c**（u/v 不落盘）
- 验收：CPU+SIM `c` max=0；SIM ~626k tick；`SEED_D=20260619`
- 家里续测：`git pull` 后直接 `bash run.sh -r cpu|sim -v Ascend910B4`

## 4. 高价值工程债 1–3（**保留**）

用户确认先做改进清单高价值 1–3（中等暂缓）。**已落地，与路线 11 无关，回滚路线 11 时须保留**：

1. **`run.sh` 资源友好化（T7b 合入全链）**：默认 `ENCRYPT_SKIP_REBUILD=1`、`CMAKE_BUILD_JOBS=2`；stamp 含 RUN_MODE+主要宏；`ENCRYPT_FORCE_REBUILD=1` 强制全量。对齐 alg20。
2. **`gen_data.py` 自包含**：优先复用 correctness 产物；缺失时本目录 `gen_ek_pke(SEED_D)` + `rng(SEED_D+991)`→m/coins + `golden_encrypt`→c；**仍写** `lut_*.bin`（供 host `ReadFile`→ws）。
3. **文档口径**：STATUS/PLAN/run.sh/main/layout 统一为 **FIPS 行 1–22 完整 Encrypt**；删「vendor compute 待建」等过时结构图。

correctness 探针的 T7b 对齐仍待。

## 5. 性能路线 11（LUT H2D→ROM）— **关闭**

曾尝试去掉 `lut_*.bin` / 设备直读常量区；用户裁定不值得继续（改后数分钟无输出，不应靠拉长 900s 预算硬等）。

| 子方案 | 结果 | 说明 |
|--------|------|------|
| A. host 静态表 → `memcpy` ws → H2D | **放弃** | SIM 数分钟无输出；保守段还误引入 `l18_l19` **重复 `FsmWait(ST_NTT_AIV_SPLIT)`** → 死锁 |
| B. AIC 直读 `__gm__ const` 作 MMAD B | **证伪** | SIM launch 2 挂死 ~900s timeout + core dump |
| 回滚 | **仅回滚路线 11** | 删除 `f203_lut_*.inc` / ROM host 头；恢复 `ReadFile(lut_*.bin)`；**保留 §4 的 1–3** |

**实验纪律**：基线 SIM **~626k tick、墙钟 ~100s 内**；若改后需拉长防挂死预算或长时间无日志，即视为退化。

**当前 LUT 路径**：host `ReadFile(lut_*.bin)` → ws → H2D（与 PASS 基线一致）。LUT H2D→ROM **不再作为本探针优化项**。

## 6. examples 晋级：Alg.14 Encrypt customspec（同日）

新建 [`examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4/`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4/)：

- **规格**：[`exp-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-encrypt-k4/exp-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.tex)（+ PDF）
- **I/O**：`ek_pke`+`m`+`coins` → **仅** `c`；禁止 Â/y/u/v 等中间态落盘
- **Launch**：SIM 2 / CPU 5；基线探针 `pass-fix-f203-alg14-pke-encrypt-device-k4`
- **状态**：`$写规格$` 已闭环；**【预研】写码**已落地 — CPU+SIM `c max=0`（SIM tick **627614**）；`output/` 仅 `c.bin`

## 7. `#交付#`：liboqs KAT + roundtrip → stable（同日）

用户门槛：两项门禁都过再晋级。

| 门禁 | 脚本 | 结果（2026-07-09） |
|------|------|-------------------|
| liboqs ↔ AscendC Encrypt | `examples/.../exp-fips203-mlkem-pke-encrypt-k4/kat_liboqs_vs_ascendc.sh` | **CPU×10 + SIM×1 PASS** |
| device Encrypt→Decrypt 闭环 | `scripts/roundtrip_pke_batch.sh`（Encrypt 默认本算子） | **CPU×10 + SIM×1 PASS** |

落地：

- `scripts/prepare_kat_input.py`：外部 ek/m/coins → input + `golden_v` + golden/c
- `run.sh`：`ENCRYPT_KAT` / `ENCRYPT_SKIP_GEN_DATA`（KAT/roundtrip 不覆盖 fixture）
- `scripts/roundtrip_pke_encrypt_decrypt.sh`：默认 Encrypt → **stable**（晋级后）
- **复制晋级** [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/)
- baseline-registry：[`docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md`](../../docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md)

**说明**：无 NPU 实机前，**SIM 为交付主参考**；CPU 依赖 `golden_v` 注入，仅作辅助正确性孪生（非与 SIM 同构）。见 [交付口径笔记](../../docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)。

## 8. 验收权重定稿（同日晚间）

用户确认：

- **CPU**：辅助正确性测试（快回归 / 多轮种子扫）。
- **SIM**：当前无 NPU 时的**主要参考点**（同构全链、tick、交付结论）。
- 已刷新：`docs/notes/F203-Alg14-Encrypt-交付口径-…`、stable/exp `STATUS`/`SELF_CONTAINED`、分叉指南 §1、baseline-registry、`AGENT_HANDOFF`、索引与 TODO。

## 9. 目录改名：`*-fips203-mlkem-pke-*`（同日）

用户要求 `examples/incubating` / `examples/stable` 目录统一为 `*-fips203-mlkem-pke-*`（**保留** `exp-sepolyvec8-ntt-k8`）。

| 旧名 | 新名 |
|------|------|
| `exp-mlkem-f203-pke-{keygen,encrypt}-k4` | `exp-fips203-mlkem-pke-{keygen,encrypt}-k4` |
| `exp-mlkem-f203-alg13-16171820-2s1e-k4` | `exp-fips203-mlkem-pke-alg13-16171820-2s1e-k4` |
| `exp-mlkem-f203-stage{1,3}-*` | `exp-fips203-mlkem-pke-stage{1,3}-*` |
| `stable-mlkem-f203-pke-{keygen,encrypt}-k4` | `stable-fips203-mlkem-pke-{keygen,encrypt}-k4` |

**未改**：derand 域分隔符 `exp-mlkem-f203-2s1e-k4:SEED_D=`（密码学契约）；`../../ascendc-tests/` 探针名；`exp-sepolyvec8-ntt-k8`；`examples/frozen/` 历史目录名（判决书「原路径」字段保留）。

**引用同步（同日续）**：`.cursor/skills/{INDEX,ascendc-impl-spec}` Stage1/3 → `exp-fips203-…`；Stage12 customspec / API 查阅索引 / `docs/notes/F203-Stage12*` 中 incubating 死链 → `examples/frozen/frozen-exp-…`。

## 10. Alg.15 Decrypt 优化探针开工（同日）

新建 [`../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4/)（**P0 仅方案**）。

| 锁定 | 说明 |
|------|------|
| 标准 | FIPS 203 Alg.15：`m ← ByteEncode₁(Compress₁(w))`（非 Decode） |
| Compress₁ | **尝试向量化**（Barrett `1290168` / `<<30` / `>>31`，对拍 liboqs） |
| ByteEncode₁ | **标量** pack（256bit→32B） |
| 编排 | **不**第一版单 kernel；**最终目标** SIM 单 kernel；先 P1 尾积木 → P2 优化 2-launch → P4 尝试单 kernel |
| 基线 | G4 correctness ~427k；SIM 主参考 / CPU 辅助 |

TODO：**T15b**；晋级仍 **T15a**。

### 10.1 P1 首版写码结果（同日）

- 自 G4 脚手架复制；尾段新写 `compute/compress1_byteencode1/`（向量 Compress₁ + 标量 Encode₁）。
- **CPU+SIM `m` max=0**；SIM **Total tick 417697**（G4 ~427k）。
- **定点**：golden/设备对齐 **liboqs Barrett**；旧 G4 `(Q+1)/2` 在 u=832 差 1 bit（已不跟 G4）。

### 10.2 P2：prep 向量 Decompress + 禁止 mid D2H（同日）

用户要求：先全向量化路线（Encode₁ 可标量）；**先改 prep**；**生产禁止中间态 D2H**，全部 device。

| 变更 | 说明 |
|------|------|
| unpack | ByteDecode 标量 + Decompress₁₁/₅ 向量（`DECOMPRESS_D_VEC=1`） |
| Host | 仅 D2H `m`；默认 `DECRYPT_GATE=0` |
| SIM | **368842** tick（相对 P1 降约 5 万） |

待：`v−w` 向量化、ŝ decode、单 kernel。

### 10.3 P3：单 kernel + v−w 向量（同日）

用户纠正：除 **ByteEncode₁** 外都应向量（含 **`v−w`**）；下一步即 **单 kernel launch**。

| 变更 | 说明 |
|------|------|
| 尾 | `Sub` + `wrap_mod_q_vec`（v−w）；Compress₁ 向量；Encode₁ 标量 |
| Kernel | **`f203_decrypt_device_fused`** 1 launch；GATE 4/8；softSyncGm（prep/su_dot 仅 AIV0） |
| GM 可见性 | `decode_s_hat` / `pad_w_hat` 改 UB+`DataCopy`/`Duplicate`（禁标量写 GM→MTE 读） |
| 验收 | CPU+SIM `m` max=0；SIM tick **295775**（P2 368842 → −20%） |

下一步候选：Decode 向量化、su_dot→INTT UB 驻留、晋级 stable（T15a）。

### 10.4 尾段 Compress₁+Encode₁ 轻量融合（同日）

| 变更 | 说明 |
|------|------|
| 融合 | Compress 原地写 `wCan`；Encode 每 8 lane→1B；**去掉**独立 `bits[256]` UB |
| 验收 | CPU+SIM `m` max=0；SIM tick **283307**（融合前 295775，约 −4%） |
| 未做 | Gather 真·向量 bit 流 |

### 10.5 su_dot→INTT UB 驻留实验 → **两档均已回滚**（同日）

| 档 | 做法 | SIM tick | 结论 |
|----|------|----------|------|
| **A** | `wHat→另 pipe pad→ProcessFromLocal` | **287680 / 287687 / 287700** | vs 基线 **283278** 约 +1.5% |
| **B 同 TPipe** | `ProcessToPadUb`（ŵ 在 su_dot 同 pipe）→`ProcessFromLocal` | **287463 / 287411** | **仍无收益**（与 A 同量级） |
| CPU | 两档 `ProcessFromLocal` 对拍失败 | — | 须分叉 |

| 判决 | **回滚**生产路径；本段 UB 驻留 **关闭**（再开须 Stage1 并入同 pipe 且证明 tick 下降） |

### 10.6 仓库闭环脚本默认 Decrypt → device-k4（同日）

| 脚本 | 变更 |
|------|------|
| `roundtrip_pke_encrypt_decrypt.sh` / `roundtrip_pke_batch.sh` | 默认 `DECRYPT_DIR` → `pass-fix-f203-alg15-pke-decrypt-device-k4`；`DECRYPT_GATE=0`；**KeyGen 默认 → `stable-fips203-mlkem-pke-keygen-k4`**（Encrypt 已是 stable） |
| `liboqs_pke_vs_ascendc.sh` | Decrypt 默认 device-k4（KeyGen 仍探针：cmake 路径不同） |
| 回退 | `KEYGEN_DIR=.../pass-fix-f203-alg13-device-keygen-k4`；`DECRYPT_DIR=.../stable-fips203-mlkem-pke-decrypt-k4` |
| 验收 | CPU+SIM roundtrip `m` max=0（`SEED_D=20260619`；SIM Decrypt tick **283248**） |

### 10.7 探针改名 `pass-fix-…-device-k4`（同日）

`fix-f203-alg15-pke-decrypt-device-k4` → **`pass-fix-f203-alg15-pke-decrypt-device-k4`**（终态；T15b 关闭；待 T15a 晋级）。

### 10.8 Decrypt `$写规格$` → incubating customspec（同日）

| 项 | 内容 |
|----|------|
| 目录 | [`examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4/`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4/) |
| 规格 | [`exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4/exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex) / PDF |
| 基线 | vendor 自 `pass-fix-f203-alg15-pke-decrypt-device-k4`；I/O dk+c→仅 m；1 launch MIX `aicore=1`；tick ~283k |

### 10.9 Decrypt 【预研】写码 PASS（同日）

| 项 | 结果 |
|----|------|
| 目录 | [`exp-fips203-mlkem-pke-decrypt-k4`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-pke-decrypt-k4/) |
| 动作 | 自 `pass-fix-…-device-k4` 一次性 vendor；切断默认 `vendor_sync`；incubating `REPO_ROOT` |
| CPU | `m` max=0 |
| SIM | `m` max=0；tick **283290** |

### 10.10 Decrypt 注释 + Alg.15 生产 I/O 收紧（同日）

| 项 | 内容 |
|----|------|
| 范围 | `exp-fips203-mlkem-pke-decrypt-k4` **与** `pass-fix-f203-alg15-pke-decrypt-device-k4` 同步 |
| 注释 | 主路径补详细中文：`main_decrypt*`、`f203_decrypt_device_fused_entry`、unpack/decode/su_dot/tail、`gen_data.py`、`layout.h` |
| I/O 判决 | 对照 Alg.15：**生产** `input/` 仅 `dk_pke`+`c`+`lut_*`；`output/` 仅 `m`（+对拍 `golden_m`） |
| 曾多余 | `ek_pke`/`m`/`coins`/`meta` 误留 `input/`（造 c 夹具）→ 改写 `output/_gen_fixture/`，`gen_data` 末尾 scrub |
| 家里续 | **KAT** + **round-trip**（`DECRYPT_DIR`→exp）+ **`#交付#` stable**（T15a） |
