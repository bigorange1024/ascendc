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
| `fix-f203-alg19-kem-keygen-correctness-k4` | PASS | PASS | 742486 |
| `fix-f203-alg20-kem-encaps-correctness-k4` | PASS | PASS | 1029501 |
| `fix-f203-alg21-kem-decaps-correctness-k4` | PASS | PASS | 985313 |

### Cloud 再修（同 PR `cursor/ntt-onnx-pat-retry-5334`）

| 根因 | 处理 |
|------|------|
| alg19 `kVecTilingBytes` 未用 → Clang `-Werror` | 删除 |
| alg20/21 `kUTrBytes` 未用 | 删除 |
| alg20 命名空间 constexpr 仅 CPU 用、SIM 侧局部遮蔽 → `-Wunused` | 包进 `#ifdef ASCENDC_CPU_DEBUG`；SIM 仍共用 `kIntt/G4/PackBlockDim` |
| alg21 SIM 无条件编入未引用的 `vendor/.../main_encrypt_g5_run.cpp` | 默认不再编入；`KEM_COMPILE_ENCRYPT_G5_HOST=1` 可显式打开 |

**状态**：六探针复验 **完成**；ntt_onnx PAT 路径 **可用**。