# ascendc — AscendC + PQC 研究型工程

## 工程目标

在 **AscendC** 上探索 **PQC（后量子密码）** 相关算子的实现与验收；本机为 **WSL2、无昇腾实机**，通过 **CPU 孪生 / CAModel 仿真**（`run.sh`）验证算子 **I/O 与 golden 一致**（不要求与基准源码同构）。

| 阶段 | 代码位置 | 说明 |
|------|----------|------|
| **预研** | `examples/incubating/ml-kem/ml-kem-*/exp-<名>/` 等 | 方案未定时的试验代码**只写 incubating**；ML-KEM 按参数组分子目录 |
| **定型** | `examples/stable/ml-kem/ml-kem-*/stable-<名>/` 等 | 从活跃 `exp-*` **复制**晋级；修订用新版本号 |
| **功能探针** | `ascendc-tests/ml-kem/ml-kem-*/` 或本层玩具 | 平台验证（含 `add_custom` 冒烟）；**无** customspec、**不**晋级 stable |
| **路线关闭** | `ascendc-tests/frozen/`、`examples/frozen/` | **否决判决**；可进目录读 `FROZEN.md`/INDEX；**禁止把代码/路线带出** |

## 研究型仓库：路线会大量关闭

本工程**允许试错**，探针与 `exp-*` 会很多；**大多数路线最终会废弃**并迁入 `frozen/`。  
**活跃代码**以各目录 `INDEX.md` 为准。`frozen/` 可进入阅读**关闭说明**（`FROZEN.md` 等），了解后退出；**禁止**把其中源码或路线带入活跃目录。  
治理说明：[docs/notes/研究路线与frozen治理.md](docs/notes/研究路线与frozen治理.md) · Rule：[ascendc-development.mdc](.cursor/rules/ascendc-development.mdc) §`**/frozen/`**

## 新 Agent 阅读顺序

1. **本文件** — 目标与目录结构  
2. **[AGENTS.md](AGENTS.md)** — **Cloud / coding agent 短入口**（硬门禁 + 文档地图；与 HANDOFF 一并维护）  
   - Cloud 云端 Agent 另见 **[Cursor-Cloud环境说明.md](Cursor-Cloud环境说明.md)**（非 WSL VM 的启动/运行坑与 SIM 绕过）  
3. **[AGENT_HANDOFF.md](AGENT_HANDOFF.md)** — **办公室 ↔ 家里 Agent 每日交接**（当前真相、下一 P0、smoke）  
4. **[qa/INDEX.md](qa/INDEX.md)** — 近期讨论与遗留（**[qa/TODO.md](qa/TODO.md)**）  
5. **[.cursor/rules/ascendc-development.mdc](.cursor/rules/ascendc-development.mdc)** — 全仓库底线（含 **frozen 禁止抄码**）  
6. **[docs/notes/研究路线与frozen治理.md](docs/notes/研究路线与frozen治理.md)** — frozen：进门读判决书，出门不带码  
7. **[.cursor/skills/INDEX.md](.cursor/skills/INDEX.md)** — 场景手册（`【】`→预研，`#…#`→交付）  
8. 环境复现：**[docs/engineering/环境复现与开发指南.md](docs/engineering/环境复现与开发指南.md)**（§12 Prompt、§14 测试矩阵）  
9. **ML-KEM NTT**：**[docs/notes/MLKEM-NTT-向量与标量实现指南.md](docs/notes/MLKEM-NTT-向量与标量实现指南.md)** — 活跃探针 `vec-k4-v2`；标量对照组已归档 `frozen-fix-f203-2s1e-alg13-16171820-k4`

## 顶层目录结构

```text
~/ascendc/
├── README.md                 # 本文件：目标 + 结构（子目录不设 README）
├── AGENTS.md                 # Cloud / coding agent 短入口（硬门禁；随工程刷新）
├── AGENT_HANDOFF.md          # 办公室 ↔ 家里 Agent 每日交接（当前真相、smoke）
├── .cursor/
│   ├── rules/                # Rule（变更须用户确认）
│   └── skills/               # ascendc-impl-spec、pre-research、ascendc-delivery
├── docs/                     # 项目产出 → research/ specs/ engineering/ notes/ reports/
├── qa/                       # INDEX.md、TODO.md；日纪要仅在 YYYY-MM/YYYY-MM-DD-关键词.md
├── library/                  # 外部资料 + shared/（探针共用代码与 vendored 设备原语）
├── thirdparty/               # 外部依赖（不进 Git；见 docs/engineering/thirdparty-本地依赖.md）
│                             # 换机：bash scripts/clone-thirdparty.sh（默认 build liboqs）
├── ascendc-tests/            # 平台功能探针（见 INDEX.md）
│   └── ml-kem/
│       ├── ml-kem-512/       # ML-KEM-512 活跃探针（W0/W1 全绿，W2 D13/D14/D15 已绿，W3 D19/D20 已绿）
│       ├── ml-kem-1024/      # ML-KEM-1024 活跃探针（frozen 仍在 frozen/）
│       └── ml-kem-768/       # ML-KEM-768 探针（W0–W3 全绿）
├── examples/
│   ├── incubating/ml-kem/ml-kem-1024/exp-*/   # 研究中（按参数组）
│   ├── incubating/ml-kem/ml-kem-512/           # ML-KEM-512 incubating 壳（写码须 customspec）
│   ├── incubating/ml-kem/ml-kem-768/exp-*/    # 768 incubating（W4+glue 已绿）
│   └── stable/ml-kem/ml-kem-1024/stable-*/    # 定型（按参数组；无 768）
├── src/  include/  Makefile  # 普通 C（唯一 main：src/main.c）
├── scripts/                  # env.sh、verify-cann.sh、clone-thirdparty.sh、roundtrip_*
│                             # 含 exp_kem768_liboqs_roundtrip.sh（768 AscendC-only）

├── backup-project.sh         # 备份前须先刷新 INDEX 与本 README；含 ascendc-tests/、examples/ 用例树（排除 build/OPPROF/dump 等产物）
└── packages/  samples/       # Gitee ascend/samples（参考用，含 AscendC MatmulLeakyRelu 等）
```

各目录 **`INDEX.md`** 说明「何时读什么」；增删文件时须同步更新。备份前须先刷新索引与本 README，再运行 `./backup-project.sh`。

---

## 运行环境（WSL / 无 NPU）

在 **WSL2 + Ubuntu 22.04 x86_64** 上搭建 AscendC 学习与调试环境（无昇腾实机）。

**完整复现与测试矩阵**：[docs/engineering/环境复现与开发指南.md](docs/engineering/环境复现与开发指南.md)。

**技术总结模板**：[docs/notes/技术总结写作模板.md](docs/notes/技术总结写作模板.md)。

## 当前状态

| 项目 | 说明 |
|------|------|
| CANN 社区版 | **9.0.0**（Toolkit + 910-ops） |
| 安装路径 | `~/Ascend/cann` → `~/Ascend/cann-9.0.0` |
| 驱动 | 未安装（WSL 无 NPU，正常） |
| 验证 | `verify-cann.sh` 已通过 |
| KeyGen 交付 | [`stable-fips203-mlkem-pke-keygen-k4`](examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/) **CPU/SIM/KAT ✓**（SIM **542393**）；探针 [`pass-fix-f203-alg13-device-keygen-k4`](ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg13-device-keygen-k4/) |
| Encrypt 交付 | [`stable-fips203-mlkem-pke-encrypt-k4`](examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) **SIM 主参考** tick **627590** · CPU 辅助 · KAT×10+1 / roundtrip×10+1 ✓；[交付口径](docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md) |
| Decrypt 交付 | [`stable-fips203-mlkem-pke-decrypt-k4`](examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) **CPU/SIM/KAT ✓**（SIM **283290** tick）；KAT×10+1 / roundtrip×10+1 ✓ |
| KEM KeyGen | **定型** [`stable-fips203-mlkem-kem-keygen-k4`](examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/)（2026-07-14 `#交付#`；SIM ~707k）；预研副本 [`exp-…`](examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-keygen-k4/)；行为基线探针 [`pass-fix-f203-alg19-kem-keygen-device-k4`](ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4/)（~713k） |
| KEM Encaps | **定型** [`stable-fips203-mlkem-kem-encaps-k4`](examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/)（2026-07-15 `#验收#`；SIM **721119**）；预研副本 [`exp-…`](examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4/)；行为基线 [`pass-fix-…-encaps-device-k4`](ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/)（**721010**） |
| KEM Decaps（交付） | **定型** [`stable-fips203-mlkem-kem-decaps-k4`](examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4/)（`#交付#` + **T19i SIM 3**；tick **1050620**）；预研副本 [`exp-…`](examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-k4/)；基线 [`pass-fix-…-decaps-device-k4`](ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/)；`scripts/` 默认 |
| KEM Decaps（CT 专题） | [`stable-…-decaps-ct-k4`](examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-ct-k4/) · [`exp-…-decaps-ct-k4`](examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-decaps-ct-k4/) · [`pass-fix-…-decaps-device-ct-k4`](ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-ct-k4/)（`research/formal-lang-dag`；第7章 CT / 五指标；**非** scripts 默认） |
| **ML-KEM-768（k=3）** | **有条件完成至 incubating**：探针 W0–W3 + E13–E21ct + AscendC-only RT；参数卡 [`fips203-mlkem768-parameter-card.md`](docs/specs/fips203-mlkem768-parameter-card.md)；**无** stable-768；纪要 [`2026-07-27`](qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |
| **ML-KEM-512（k=2）** | **P0/P1 已定稿，W0/W1 全绿，W2 D13/D14/D15 已绿，W3 D19/D20 已绿**：B2 [`pass-fix-f203-byteencode-decode-d-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-byteencode-decode-d-k2/) CPU/SIM ✓（enc4 **5407** / dec4 **9340** / enc10 **6629** / dec10 **6561** / enc12 **17613**）；B3a/B3b SIM **11377** / **13566**；B4 [`pass-fix-f203-alg7-sample-ntt-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg7-sample-ntt-k2/) SIM **80235**；B5 [`pass-fix-f203-stage123-ntt-intt-polyvec4-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-stage123-ntt-intt-polyvec4-k2/) NTT **22921** / INTT **22836**；B6 [`pass-fix-f203-alg11-12-multiply-inner-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg11-12-multiply-inner-k2/) multiply **9290** / inner **12603**；D13 [`pass-fix-f203-alg13-device-keygen-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg13-device-keygen-k2/) SIM **230102**；D14 [`pass-fix-f203-alg14-pke-encrypt-device-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg14-pke-encrypt-device-k2/) SIM **338153**；D15 [`pass-fix-f203-alg15-pke-decrypt-device-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg15-pke-decrypt-device-k2/) SIM **168975**；D19 [`pass-fix-f203-alg19-kem-keygen-device-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg19-kem-keygen-device-k2/) SIM **320247**；D20 [`pass-fix-f203-alg20-kem-encaps-device-k2`](ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg20-kem-encaps-device-k2/) SIM **394978**；禁 stable-512 |
| Host 随机 | 已正确性 PKE/KEM：**默认** [`fips203_host_rng`](library/shared/fips203_host_rng/) SHA3/SHAKE 派生；`SEED_D=` 定点可覆盖 |
| KEM 三分项 | **Alg.19/20/21** device+stable **均已绿**（交付 Decaps **无 `-ct`**）；办公室↔liboqs：[`scripts/stable_kem_liboqs_roundtrip.sh`](scripts/stable_kem_liboqs_roundtrip.sh)（urandom→同字节喂 AscendC；**CPU+SIM 全绿**）；correctness×3 **已冻结**；见 [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) · [`qa/TODO.md`](qa/TODO.md) |
| 统一整数 Compress/Decompress | exp [`exp-fips203-compress-unified-int-vec-k4`](examples/incubating/ml-kem/ml-kem-1024/exp-fips203-compress-unified-int-vec-k4/) · [`exp-fips203-decompress-unified-int-vec-k4`](examples/incubating/ml-kem/ml-kem-1024/exp-fips203-decompress-unified-int-vec-k4/)（customspec）；生产路径已迁入 stable PKE Encrypt/Decrypt；**PKE round-trip PASS** |
| 官方样例 | `samples/` ← [gitee.com/ascend/samples](https://gitee.com/ascend/samples) `master`（`6511a5f`，2026-06 拉取）；AscendC Cube+Vector 融合参考：`samples/operator/ascendc/tutorials/MatmulLeakyReluCustomSample/` |

## 普通 C 演示（根目录）

与 AscendC 无关，源码布局：

- `src/main.c` — 工程唯一 `main`
- `src/*.c` — 其它实现
- `include/*.h` — 头文件

```bash
cd ~/ascendc
make && make run
```

### VS Code 编译 C 工程（推荐，不依赖 C/C++ Runner）

本仓库含巨大的 `samples/` 目录，**C/C++ Runner** 扫描文件夹时容易卡死，导致「Select folder」、状态栏 **ascendc** 按钮点击无反应。**建议不要用 Runner 的齿轮/文件夹按钮**。

**推荐方式：**

| 操作 | 方法 |
|------|------|
| 编译 | **`Ctrl+Shift+B`**（默认 Makefile → `build/app`） |
| 运行 | **`Ctrl+Shift+P`** → **Tasks: Run Task** → **C: Run main (build/app)** |
| 调试 | **F5** → **Debug main (src/main.c → build/app)** |

状态栏 Runner **▶** 也可用；若终端一闪而过，用上面 **Run Task**，终端会保留并打印 `[done]`。

**C/C++ Runner 编译报 `demo.h: No such file or directory`：**

扩展初始化时会把 `C_Cpp_Runner.includePaths` 清空。本仓库已在 `.vscode/settings.json` 中写好：

- `includePaths`: `${workspaceFolder}/include`
- `compilerArgs`: `-I${workspaceFolder}/include`

状态栏文件夹选 **`src`** 后点齿轮。若仍报错，检查 `settings.json` 里上述两项是否被扩展改回 `[]`。

**若仍想用 C/C++ Runner：**

1. 状态栏文件夹选 **`src`**（不是 ascendc 根）
2. 点 **⚙️ Start Compilation**
3. 运行/调试用状态栏 **▶** / **Debug**（产物在 `build/Debug/outDebug`）

仍卡死时：禁用 Runner，用 **`Ctrl+Shift+B`**（`Makefile`，产物 `build/app`）。

## 日常使用

新开终端后（`.bashrc` 已配置）环境会自动加载。若 `ccec` 找不到：

```bash
source ~/ascendc/scripts/env.sh
```

检查环境：

```bash
bash ~/ascendc/scripts/verify-cann.sh
```

预期输出包含 `version=9.0.0`、`ccec`、`tikicpulib: OK`、`simulator libs: 5 files`。

### 环境变量

| 变量 | 典型值 |
|------|--------|
| `CANN_HOME` | `~/Ascend/cann` |
| `ASCEND_HOME_DIR` | 同上 |
| `ASCEND_TOOLKIT_HOME` | 由 `set_env.sh` 设置 |

**不要**在脚本里直接 `source ~/Ascend/cann/set_env.sh` 且同时 `set -u`，会触发 `unbound variable`。统一用 `~/ascendc/scripts/env.sh`。

## CANN 安装路径（本机）

```text
~/Ascend/
├── cann -> cann-9.0.0
├── cann-9.0.0/
└── ascend-toolkit/
```

## 开发能力（无实机）

| 能力 | 路径 / 工具 | 需要 NPU |
|------|-------------|----------|
| AscendC 编译 | `ccec`（`$CANN_HOME/bin`） | 否 |
| CPU 孪生调试 | `tools/tikicpulib/lib/<芯片>/`（如 `Ascend910B4`） | 否 |
| NPU 仿真 (CAModel) | `toolkit/tools/simulator/<芯片>/lib/` | 否 |
| 上板运行 | 需真机 + 驱动 | 是 |

CANN 9.0 文档：[版本说明](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/releasenote/release-notes.md) · [快速安装](https://www.hiascend.com/cann/download)

内置 AscendC 样例与文档：

```bash
ls ~/Ascend/cann-9.0.0/x86_64-linux/ascendc/
```

本仓库 Add 算子示例（**CANN 9.0，WSL 无 NPU 已跑通**，详见下文与 [环境复现与开发指南](docs/engineering/环境复现与开发指南.md) 第 8、14 节）。

`ascendc-tests/add_custom/run.sh` **开头已自动** `source ~/ascendc/scripts/env.sh`，一般**不必**先手动 source。

```bash
cd ~/ascendc/ascendc-tests/add_custom

./run.sh cpu 910B4              # CPU 孪生 + golden（WSL 日常首选，约 10s）
./run.sh sim-build 910B4        # 仅编译 sim 二进制，验证 camodel 链接
./run.sh sim 910B4              # 默认：msprof → prof_sim/ + OPPROF_*（慢，数分钟～数十分钟）
SIM_DIRECT=1 ./run.sh sim 910B4 # 仅 CAModel 跑算子 + golden，不生成 OPPROF_*（约 20s）
./run.sh sim 910B4 SIM_DIRECT=1 # 同上（第三参数写法）

./run.sh npu 910B4              # 实机上板（需 NPU）
RUN_WITH_MSPROF=1 ./run.sh npu 910B4
```

### SIM_DIRECT：是否生成 `OPPROF_*`（sim 专用）

| `SIM_DIRECT` | 命令 | msprof | `OPPROF_*` |
|--------------|------|--------|------------|
| **0**（默认） | `./run.sh sim 910B4` | ✅ | ✅（成功时） |
| **1** | `SIM_DIRECT=1 ./run.sh sim 910B4` 或 `./run.sh sim 910B4 SIM_DIRECT=1` | ❌ | ❌ |

性能数据在 `OPPROF_*/simulator/` 与 `prof_sim/`（`prof_sim/latest` 软链指向最新 `OPPROF_*`）。`msprof` 可能以 `750` 权限建目录，`run.sh` 结束后会 `chmod` 便于 IDE 浏览。

### 已完成的 AscendC 运行测试（Agent 复现清单）

在 **WSL2 Ubuntu 22.04 x86_64、无 NPU** 上，对 `ascendc-tests/add_custom` 已验证：

| 步骤 | 命令 | 成功标志 |
|------|------|----------|
| 1 | `bash ~/ascendc/scripts/verify-cann.sh` | `All checks passed`，`version=9.0.0` |
| 2 | `./run.sh cpu 910B4` | `[SUCCESS] output matches golden (Ascend910B4)` |
| 3 | `./run.sh sim-build 910B4` | `ldd ./add_custom_npu \| grep runtime_camodel` 有输出 |
| 4 | `SIM_DIRECT=1 ./run.sh sim 910B4` | golden 成功；**无**新 `OPPROF_*` |
| 5 | `./run.sh sim 910B4` | golden 成功；生成 `OPPROF_<时间戳>_*`、`prof_sim/latest` |

已验证主机：`BLUEZONE-03`（公司 WSL，源环境）、`HP-SPECTRE-FM06J2B`（家用 WSL，2026-05-24）。两台机器共用本仓库 `run.sh` / 文档；2026-05-26 在 BLUEZONE-03 复跑 T1–T4 通过。

**公司电脑 Agent**：请读 [docs/engineering/环境复现与开发指南.md](docs/engineering/环境复现与开发指南.md) **§12 一键 Prompt** 与 **§14 运行测试记录**，按顺序执行即可复现。

可用设备名见 `ascendc-tests/add_custom/RUN.md`；解析脚本：`ascendc-tests/add_custom/scripts/resolve_device.sh`。

## 样例代码说明

`~/ascendc/samples/` 来自 [Ascend/samples](https://github.com/Ascend/samples)，其中部分旧样例（如 `kernel_invocation/Add`）与 9.0 头文件不完全兼容，勿直接照搬 `run.sh`。

建议优先使用：

1. CANN 9.0 自带：`~/Ascend/cann-9.0.0/x86_64-linux/ascendc/`
2. 昇腾社区与 9.0 配套的算子样例（[cann/download](https://www.hiascend.com/cann/download) 选版本后获取）

## 重装 CANN 9.0.0

先卸载（若已安装）：

```bash
cd ~/ascendc/packages
./Ascend-cann-910-ops_9.0.0_linux-x86_64.run --uninstall --quiet
./Ascend-cann-toolkit_9.0.0_linux-x86_64.run --uninstall --quiet
rm -rf ~/Ascend/cann-9.0.0 ~/Ascend/cann ~/Ascend/ascend-toolkit
```

再安装（二选一）：

**脚本一键安装**（官方 OBS 直链，约 3.5GB）：

```bash
bash ~/ascendc/scripts/install-cann-9.0.sh
```

**手动 wget + 安装**：

```bash
mkdir -p ~/ascendc/packages && cd ~/ascendc/packages
wget -c "https://ascend-repo.obs.cn-east-2.myhuaweicloud.com/CANN/CANN%209.0.0/Ascend-cann-toolkit_9.0.0_linux-x86_64.run"
wget -c "https://ascend-repo.obs.cn-east-2.myhuaweicloud.com/CANN/CANN%209.0.0/Ascend-cann-910-ops_9.0.0_linux-x86_64.run"
chmod +x Ascend-cann-*.run
./Ascend-cann-toolkit_9.0.0_linux-x86_64.run --install --quiet --feature=ascendc
./Ascend-cann-910-ops_9.0.0_linux-x86_64.run --install --quiet
source ~/ascendc/scripts/env.sh
bash ~/ascendc/scripts/verify-cann.sh
```

也可使用 [pip / apt 在线安装](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/softwareinst/instg/instg_0000.html)（Ubuntu 选 `netapt` / `netpip`）。

## 系统依赖

- 已用用户级 `pip`（`~/.local/bin`），未依赖 sudo。
- 编译需要：`gcc`、`g++`、`cmake`（Ubuntu 22.04 一般已具备）。
- 磁盘：安装目录建议预留 **≥10GB**。

可选（需 sudo）：

```bash
sudo apt-get install -y python3-pip gcc g++ make cmake libssl-dev zlib1g-dev
```

## 历史说明

本环境曾安装 CANN **8.2.RC1**，已卸载并升级至 **9.0.0**。旧脚本 `scripts/install-cann.sh` 仅对应 8.2，请勿再使用。
