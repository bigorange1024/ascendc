# 2026-07-14 — KEM KeyGen incubating【预研】重写 + 多轮 KAT

## 结论

`examples/incubating/exp-fips203-mlkem-kem-keygen-k4` 按 customspec（含 SyncAll 踩坑）从零重写完成；**有条件完成**，未晋级 stable。

## 实现要点

- Vendored stable PKE KeyGen + device 行为对照的 `kem/` 尾；**自包含** CMake（禁依赖 device）。
- **SIM**：Encode 后 `SyncAll<true>` → AIV0 Fuse/Tail。
- **CPU**：禁 SyncAll；`dk_kem_gm[0:2]` soft-flag + **AIV1** Fuse/Tail（仅 AIV1 做尾仍偶发抢跑 AIV0 poly → 加旗后 40/40）。
- `KYBER_PIPE_ALL` 恒 `PipeBarrier`；VERIFY 前清零 `output/`。

## 验收

| 项 | 结果 |
|----|------|
| CPU×40（清零 output） | PASS |
| SIM_DIRECT | PASS；tick **707057** |
| vs correctness seed 20260619–28 ×10 | `cmp` OK |
| liboqs `kat_liboqs_kem_keygen.py` KEYGEN_DIR=本目录 ×10 | CPU PASS |
| liboqs **SIM×3**（`SIM_DIRECT=1`，随机 `kem_seed`） | **PASS**（三轮 ek/dk 全一致） |

## 复现纪律（用户反馈）

办公室偶发 FAIL **未落盘**具体 `kem_seed` / `SEED_D`，回家无法定点复现。以后 FAIL 须记：`mode`、`kem_seed` hex（或 `SEED_D`）、错位偏移、是否清零 `output/`。`kat_liboqs_kem_keygen.py` 已在 log 写 `kem_seed=`；压测脚本也应同样落盘。

## 推送 / 交接（同日稍后）

- 已 `git push` incubating 实现 + STATUS/SELF_CONTAINED + INDEX/TODO/AGENT_HANDOFF。
- 办公室：**未** `#交付#` 前勿建 stable；主线开 **T19a Encaps device**；见根 [`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md)。

## 办公室补充（同日）— Cloud Agent 与 `AGENTS.md`

| 项 | 内容 |
|----|------|
| 同步 | 办公室 `git pull` 已与 `origin/main` 一致（`e10655a`） |
| 新增 | 根 [`AGENTS.md`](../../AGENTS.md)：Cloud / coding agent **短入口**（硬门禁 + 文档地图） |
| 联动 | [`README.md`](../../README.md) 阅读顺序与顶层树、[`AGENT_HANDOFF.md`](../../AGENT_HANDOFF.md)、[`backup-project.sh`](../../backup-project.sh) |
| 维护 | 入口/硬门禁/文档地图变更 → 刷新 `AGENTS.md`；**每日真相**仍只写 `AGENT_HANDOFF.md` |

## 办公室补充（同日稍后）— clone 后默认 build liboqs

| 项 | 内容 |
|----|------|
| 背景 | Cloud Agent 跑 incubating KeyGen：CPU 先因缺 liboqs 挂，补编后 PASS；SIM 仍报 `libge_common_base.so … InternalSwap`（**CANN**，与 liboqs 无关） |
| 改动 | 新增 [`scripts/build-liboqs.sh`](../../scripts/build-liboqs.sh)；[`clone-thirdparty.sh`](../../scripts/clone-thirdparty.sh) **默认**调用（`BUILD_LIBOQS=0` 可关） |
| 文档 | [`AGENTS.md`](../../AGENTS.md) §换机/Cloud、[`thirdparty-本地依赖.md`](../../docs/engineering/thirdparty-本地依赖.md) |
| 纪律 | Agent **须**先 `clone-thirdparty.sh`；SIM 符号错标阻塞，勿假装装 liboqs 可修 |

## 遗留

- `#交付#` → stable（用户确认后）
- T19a Encaps device；T19f incubating 验收可关，stable 仍待交付

## 办公室补充（同日）— 多环境 `runtime_env` + `run.sh` 一期

| 项 | 内容 |
|----|------|
| 目标 | WSL / Cloud Linux / 真机 NPU 统一探测，差异进脚本不改算子语义 |
| 新增 | [`scripts/runtime_env.sh`](../../scripts/runtime_env.sh)；[`docs/engineering/NPU真机环境说明.md`](../../docs/engineering/NPU真机环境说明.md) |
| 模式 | 默认仍 **`cpu`**；新增 **`-r auto`**（npu>sim>cpu）、**`-r verify`**（cpu→`SIM_DIRECT` sim；有卡非 WSL 再 npu） |
| 试点 | KEM KeyGen incubating + PKE keygen/encrypt/decrypt 四个 `run.sh` |
| WSL 验 | decrypt：`verify` PASS（cpu+sim，tick~283k）；`auto→sim` PASS；`-r npu` 明确失败。KEM：`npu` 拒绝；`cpu` PASS |
| 明确不做（一期） | 不批量改 ~75 个 `run.sh` / frozen；不把默认改成 auto/verify |
| 二期 | 活跃 `pass-*` / 其余 exp|stable 逐步接入；真机再压 npu host 路径 |

文档挂链：[`AGENTS.md`](../../AGENTS.md)、[`Cursor-Cloud环境说明.md`](../../Cursor-Cloud环境说明.md)、`docs/engineering/INDEX.md`。

## 办公室补充（同日稍后）— Cloud 四用例测回

| 项 | 内容 |
|----|------|
| Cloud | 同步 `c0f6e6d` 后 8 轮：KEM / encrypt / decrypt CPU+SIM ✅；**stable PKE keygen** CPU+SIM 编译挂 |
| 根因 | `main_keygen.cpp` 未用 `kVecTilingBytes`；Cloud Clang `-Werror -Wunused-const-variable`（WSL GCC 常不报） |
| 修复 | 删该常量（stable + incubating/exp-pke-keygen + kem 内 vendored `main_keygen.cpp`）；WSL 强制重编 CPU+SIM PASS（tick ~542k） |

## 办公室补充（同日）— Cloud 复测绿 + 探针批量接入 runtime_env

| 项 | 内容 |
|----|------|
| Cloud 复测 | `a5693dc` 后 stable PKE keygen **CPU+SIM ✅**（tick ~542629）；四例试点 Cloud 全绿 |
| 笔记 | 新增 [`docs/notes/AscendC多环境运行纪要.md`](../../docs/notes/AscendC多环境运行纪要.md)（三环境不变量、Clang/`sim_env`/门禁） |
| 二期落地 | **活跃 `ascendc-tests/*/run.sh`** 接入 `runtime_env`（32 个；2 个 T19 device stub 跳过）；**本机不跑测**，交 Cloud Agent 跑 cpu+sim |
| 未改 | `frozen/`、默认仍 `cpu`、examples 其余非试点树 |

## 办公室补充（同日）— P0 仓内硬伤修补

| 项 | 处理 |
|----|------|
| `prepare_production_input.py` | `__future__` 提到文件 docstring 之后、其它语句之前（Cloud Python SyntaxError） |
| shake128/256 toy | 从 backup 补回 `scripts/emit_toy_active_case_h.py` |
| `pass-…-byteencode12` | 补提交缺失的 `basic.hpp` |
| `library/shared/…/fips203_prf.h` | 从 backup 恢复（verify 编 C 参考需要） |

其余：Clang `-Werror` / rsync 已在同日下一刀处理（见下）。

## 办公室补充（同日）— Cloud 失败清单修补（Clang + rsync）

| 根因 | 处理 |
|------|------|
| `-Werror=comment`（`input/*.bin` 等含字面量 `/*`） | 改注释：vec-k4-v2、polyvec8、compress/decompress、byteencode12、exp-encrypt data_utils；本机 **cpu+sim PASS** |
| Cloud 无 `rsync` | 新增 [`scripts/cp_sync.sh`](../../scripts/cp_sync.sh)；alg19/20/21 与若干 vendor_sync 改用（有 rsync 仍优先） |
| examples encrypt vendor_sync `REPO` 少上一层 | 一并改为上溯 3 层到仓根 |
| `ntt_onnx` | **仍依赖用户配 `ASCENDC_GH_PAT`**；本仓无法代授私钥仓读权限 |
| CBD 被 Cloud 标 `-Werror` | 本机 clean **cpu+sim PASS**；若 Cloud 再挂需贴原文 |

| 项 | 内容 |
|----|------|
| 决定 | **不**公开 `ntt_onnx`；曾试 public 已立刻改回 **private** |
| Cloud | Secrets 配 **`ASCENDC_GH_PAT`**（fine-grained，仅 ntt_onnx Contents:Read）；`clone-thirdparty.sh` 优先用该 PAT HTTPS 克隆并去掉 remote 中的 token |
| 勿用 | 单独依赖 `GH_TOKEN`（Cursor 可能注入仅对本仓的 `ghs_…`） |
| 复验 | Cloud：`FORCE=1 ONLY=ntt_onnx BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh` → 有 `transpose_mlkem_luts_i8.h` |

## 办公室补充（同日）— Cloud 新会话注入 PAT 后复验六探针

| 项 | 结果 |
|----|------|
| Secret | 新 Cloud run 内 `ASCENDC_GH_PAT` **已注入**（旧会话未热更新的结论成立） |
| 克隆 | `FORCE=1 ONLY=ntt_onnx BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh` → **`ntt_onnx_ok`**（`transpose_mlkem_luts_i8.h` 存在；HEAD `1c5ae1a`） |
| 附带 | alg19–21 对拍还须 `liboqs`：`ONLY=liboqs bash scripts/clone-thirdparty.sh` |

### 六探针 CPU+SIM（`Ascend910B4`，`SIM_DIRECT=1`）

| 探针 | CPU | SIM | tick（SIM） |
|------|-----|-----|-------------|
| `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | PASS | PASS | 123180 |
| `pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4` | PASS | PASS | 154538 |
| `pass-fix-f203-alg14-pke-encrypt-device-k4` | PASS | PASS | 626765 |
| `frozen-fix-f203-alg19-kem-keygen-correctness-k4` | PASS | PASS | 742486 |
| `frozen-fix-f203-alg20-kem-encaps-correctness-k4` | PASS | PASS | 1029501 |
| `frozen-fix-f203-alg21-kem-decaps-correctness-k4` | PASS | PASS | 985313 |

### Cloud 再修（同 PR `cursor/ntt-onnx-pat-retry-5334`）

| 根因 | 处理 |
|------|------|
| alg19 `kVecTilingBytes` 未用 → Clang `-Werror` | 删除 |
| alg20/21 `kUTrBytes` 未用 | 删除 |
| alg20 命名空间 constexpr 仅 CPU 用、SIM 侧局部遮蔽 → `-Wunused` | 包进 `#ifdef ASCENDC_CPU_DEBUG`；SIM 仍共用 `kIntt/G4/PackBlockDim` |
| alg21 SIM 无条件编入未引用的 `vendor/.../main_encrypt_g5_run.cpp` | 默认不再编入；`KEM_COMPILE_ENCRYPT_G5_HOST=1` 可显式打开 |

**状态**：六探针复验 **完成**；ntt_onnx PAT 路径 **可用**。

## 办公室补充（同日晚）— Cloud 二次失败清单（统一本仓修）

Cloud 反馈：polyvec8/compress/decompress 已双绿；仍挂 7 项。本仓处理：

| 探针 | 根因 | 处理 |
|------|------|------|
| CBD SIM | `kCpuLaunchBlockDim` 仅 CPU 用 → Clang `-Wunused` | 常量移入 `#ifdef __CCE_KT_TEST__` |
| byteencode12 | 缺 `data_utils.h` | 自 polyvec8 补回并改文件头 |
| alg13 se-k4 / shake | 缺 `tiny_sha3`（只 clone 了 ntt_onnx） | `scripts/ensure_thirdparty_dep.sh` + run.sh 按需 clone |
| device-keygen `SyntaxError` | `prepare` 自身 OK，但 import 的 `merged_kyber_fixed_poly` 等**双 docstring** 夹断 `__future__` | 批量把 `__future__` 挪到合法位置（9 文件） |
| shake128/256 | 缺 `CMakeLists.txt`；补回后 CPU **AIC/AIV 竞态写 reserved2**；SIM 缺 `sim_env` 致 WSL FPE；`CeilAlign32` 缺 `__aicore__ inline` 致 SIM 链接失败 | 恢复 CMake；CPU `SetKernelMode(AIV_MODE)`；run.sh 接 `sim_env_export`；CeilAlign32 标设备内联 |
| vec-k4-v2 CPU `ek_pke` | Cloud 报 diff@1272；本机 clean cpu **PASS** | **不做臆测改算法**；请 Cloud 干净重跑；若仍红贴 verify 全文 |

说明：[`docs/notes/AscendC多环境运行纪要.md`](../../docs/notes/AscendC多环境运行纪要.md)

## 办公室补充（同日晚）— CeilAlign32 恢复 constexpr

Cloud：4185ba0 后仍挂 se-k4 / device-keygen；根因 `CeilAlign32` 改成仅 `__aicore__ inline` 后无法做 `PRF_*` host constexpr。

修复：`__aicore__ inline constexpr`（host 上 `__aicore__` 为空宏 → `inline constexpr`；设备仍内联，且保持 constexpr）。本机 se/device-keygen cpu + shake128 sim PASS。
### 同日 — device-keygen `kVecTilingBytes`

Cloud：`main_keygen.cpp` 未用常量 → Clang `-Werror`。已删（与 early stable KeyGen 同类修法）。

### 同日收口 — Cloud 第二波全绿

| 项 | 结果 |
|----|------|
| 提交 | `995efdd` 上 `pass-fix-f203-alg13-device-keygen-k4` **CPU+SIM PASS**（tick **542494**；`ek_pke=1568`/`dk_pke=1536`） |
| 结论 | 此前第二波仍红用例（Clang/`__future__`/shake/tiny_sha3/CeilAlign32/kVecTilingBytes 等）**到此全部通过** |


## 家里补充（同日晚）— incubating KeyGen liboqs KAT 复测

| 项 | 内容 |
|----|------|
| 范围 | `examples/incubating/exp-fips203-mlkem-kem-keygen-k4` |
| 命令 | `KEYGEN_DIR=<本目录> KEM_KG_CPU_TRIALS=10 KEM_KG_SIM_TRIALS=1 SIM_DIRECT=1 bash scripts/liboqs_kem_keygen_batch.sh` |
| 结果 | **PASS** CPU×10 + SIM×1（旁路 A 随机 `kem_seed=d‖z` ↔ liboqs `keypair_derand` 逐字节 ek/dk） |
| 墙钟 | ≈11 min（`BATCH_EXIT:0`，stamp `20260714-185252`） |
| log | `output/liboqs_kem_keygen/kat_cpu10_sim1_20260714-185252.log`（每轮完整 `kem_seed=` hex） |
| fixture | `output/liboqs_kem_keygen/fx_20260714-185252/{cpu/1..10,sim/1}/kem_seed.bin` |

与当日 STATUS「家里 agent CPU×10 / SIM×3」独立复测一致（本轮按用户要求 SIM×1）。

## 家里补充（同日晚）— `#交付#` 晋级 stable

| 项 | 内容 |
|----|------|
| 来源 | `examples/incubating/exp-fips203-mlkem-kem-keygen-k4`（副本保留） |
| 目标 | [`examples/stable/stable-fips203-mlkem-kem-keygen-k4`](../../examples/stable/stable-fips203-mlkem-kem-keygen-k4/) |
| customspec | `stable-…-实现方案-customspec.{tex,pdf}`（已 `xelatex-clean`） |
| stable CPU | **PASS** ek/dk max=0 |
| stable SIM | **PASS** Total tick **706633**；无 stray dump |
| 索引 | `examples/{INDEX,stable,incubating}` · `README` · `AGENTS` · registry · `qa/TODO` T19f **完成** |

交付验收级别：Rule CPU + `SIM_DIRECT=1` sim。未自动 commit/push。

## 家里补充（同日晚）— add_custom `-r/-v` + KeyGen 默认 SHA3 seed

| 项 | 内容 |
|----|------|
| add_custom | `run.sh` 支持 **`bash run.sh -r cpu\|sim -v Ascend910B4`**（Cloud 通用口径）；旧 `./run.sh cpu 910B4` 兼容保留；`source env.sh` 在 `pipefail` 下临时 `set +e` |
| KeyGen seed | exp+stable：默认不写死 `20260619`；`scripts/resolve_host_seed_d.py` 用 SHA3-256 域分离标签派生 host `SEED_D`；`SEED_D=` 仍可定点 |
| 验 | add_custom `-r cpu` PASS；stable KeyGen 默认 seed `880681095` CPU ek/dk max=0 |

## 家里补充（同日晚）— examples PKE/KEM 默认哈希随机

| 项 | 内容 |
|----|------|
| 共享 | [`library/shared/fips203_host_rng/host_rng.py`](../../library/shared/fips203_host_rng/host_rng.py)：`resolve_seed_d` + `expand_bytes`（SHAKE） |
| 覆盖 | **已正确性**的 PKE KeyGen/Encrypt/Decrypt + KEM KeyGen（stable + incubating 副本） |
| 定点 | `SEED_D=20260619` 等仍可用；Encrypt 仅定点且=20260619 时才复用 frozen correctness fixture |
| 验 | 默认哈希路径 CPU：PKE keygen/encrypt/decrypt PASS |

### Cloud 测 SIM 指引（同提交推送后）

默认**勿**再写死 `SEED_D=20260619`（除非定点复现旧 KAT）。**勿**手写 `SIM_DIRECT=1`（stable/exp PKE·KEM 的 `-r sim` **已默认** CAModel 直跑）。

```bash
# 标准验收（无需额外 env）
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

cd examples/stable/stable-fips203-mlkem-pke-keygen-k4
cd examples/stable/stable-fips203-mlkem-pke-encrypt-k4
cd examples/stable/stable-fips203-mlkem-pke-decrypt-k4
cd examples/stable/stable-fips203-mlkem-kem-keygen-k4
```

定点：`SEED_D=20260619 bash run.sh …`。采 msprof/OPPROF（非默认）：`SIM_DIRECT=0 bash run.sh -r sim …`。共享 RNG：[`library/shared/fips203_host_rng/`](../../library/shared/fips203_host_rng/)。
