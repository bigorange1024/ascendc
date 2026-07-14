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