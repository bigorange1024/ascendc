# thirdparty/ — 本地依赖（不进 Git 仓）

`thirdparty/` 整树在根 `.gitignore` 中排除（`/thirdparty/`），**不会**随 `git push` / `git pull` 同步。换机或新 clone 本仓后，须拉齐外部依赖。

**一键安装（可公开 clone 的仓 + 默认编译 liboqs）**：

```bash
# clone 本仓后立刻执行（缺则 clone；默认顺带 build liboqs + kem/pke ref）
bash scripts/clone-thirdparty.sh
# 仅补编 liboqs（源码已在）：
bash scripts/build-liboqs.sh
# 只 clone 不编 liboqs：
BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh
```

清单与 URL **以本文件 + `scripts/clone-thirdparty.sh` 为准**；改上游地址时须同步改脚本内 `REPOS` 表。

---

## 总览

| 目录 | 来源 | 上游 URL | 钉住 | 用途 | 换机怎么装 |
|------|------|----------|------|------|------------|
| `tiny_sha3/` | 外部 clone | https://github.com/mjosaarinen/tiny_sha3.git | 默认分支（浅） | Host SHA3/SHAKE golden | **`clone-thirdparty.sh`** |
| `liboqs/` | 外部 clone | https://github.com/open-quantum-safe/liboqs.git | **tag `0.15.0`** | ML-KEM / PKE KAT | **`clone-thirdparty.sh`（默认再调 `build-liboqs.sh`）** |
| `ascend-samples/` | 外部 clone | https://gitee.com/ascend/samples.git | 默认分支（浅） | 昇腾官方样例，仅参考 | **`clone-thirdparty.sh`** |
| `SHA3hp/` | 外部 clone | https://openi.pcl.ac.cn/wtUSTB/SHA3hp.git | 默认分支（浅） | 第三方 AscendC Keccak/SHA3 | **`clone-thirdparty.sh`** |
| `cann-ntt/` | 外部 clone | https://openi.pcl.ac.cn/serial2007/cann-ntt.git | 默认分支（浅） | 第三方 AscendC 前向 NTT | **`clone-thirdparty.sh`** |
| `ntt_onnx/` | 外部 clone | https://github.com/bigorange1024/ntt_onnx.git（**私有**） | 默认分支（浅） | NTT/LUT golden（原 `ntt_study`） | **`clone-thirdparty.sh`** + 认证（见下） |

**已迁出 / 更名（勿再装回旧路径）**：

| 原目录 | 现落点 | 说明 |
|--------|--------|------|
| ~~`merged_kyber/`~~ | [`ascendc-tests/pass-merged-kyber-mix-ntt256/`](../../ascendc-tests/pass-merged-kyber-mix-ntt256/) | 作者授权用例化；upstream 历史：https://github.com/serial2007/MmadBiasInvocation1.git |
| ~~`ntt_study/`~~ | `thirdparty/ntt_onnx/` | 2026-07-13 起目录名与 GitHub 仓统一为 **ntt_onnx** |

期望树：

```text
thirdparty/
├── tiny_sha3/          # Host SHA3/SHAKE golden
├── liboqs/             # ML-KEM KAT（tag 0.15.0，需 build）
├── ascend-samples/     # 昇腾官方 samples（体积大）
├── SHA3hp/             # 第三方 AscendC Keccak/SHA3（OpenI）
├── cann-ntt/           # 第三方 AscendC NTT（OpenI）
└── ntt_onnx/           # NTT/LUT golden（GitHub bigorange1024/ntt_onnx）
```

---

## 可公开 clone 的六个仓（默认脚本范围）

### 命令

```bash
bash scripts/clone-thirdparty.sh
# 仅拉指定项：
ONLY=tiny_sha3,ntt_onnx bash scripts/clone-thirdparty.sh
# 已存在也强制重拉（会 rm -rf，慎用）：
FORCE=1 bash scripts/clone-thirdparty.sh
```

等价手工命令（与脚本一致）：

```bash
mkdir -p thirdparty
git clone --depth 1 https://github.com/mjosaarinen/tiny_sha3.git thirdparty/tiny_sha3
git clone --depth 1 --branch 0.15.0 https://github.com/open-quantum-safe/liboqs.git thirdparty/liboqs
git clone --depth 1 https://gitee.com/ascend/samples.git thirdparty/ascend-samples
git clone --depth 1 https://openi.pcl.ac.cn/wtUSTB/SHA3hp.git thirdparty/SHA3hp
git clone --depth 1 https://openi.pcl.ac.cn/serial2007/cann-ntt.git thirdparty/cann-ntt
git clone --depth 1 https://github.com/bigorange1024/ntt_onnx.git thirdparty/ntt_onnx
```

### `ntt_onnx` 私有仓认证（强制保持 private）

**不要**为了 Cloud / 实机把仓改成 public。匿名 HTTPS 会失败；按环境选认证即可。`clone-thirdparty.sh` 对 `ntt_onnx` 的优先级：

1. 环境变量 **`ASCENDC_GH_PAT`** 或 **`NTT_ONNX_GITHUB_TOKEN`**（HTTPS `x-access-token`）  
2. 已登录且能读该仓的 **`gh`**  
3. 匿名（必败，并打印指引）

#### A. 先做：GitHub fine-grained PAT（Cloud 与实机通用）

1. 打开 [Fine-grained personal access tokens](https://github.com/settings/personal-access-tokens/new)（须登录 **bigorange1024** 或有 `ntt_onnx` 读权限的账号）。  
2. **Token name**：例如 `ascendc-ntt-onnx-read`。  
3. **Expiration**：按安全策略选（如 90 天）；到期换新。  
4. **Resource owner**：`bigorange1024`。  
5. **Repository access** → **Only select repositories** → 勾选 **`ntt_onnx`**（不要勾整个 org）。  
6. **Permissions** → Repository permissions → **Contents** → **Read-only**（够 clone；勿给多余写权限）。  
7. Generate token → **立刻复制**保存（界面只显示一次）。

#### B. 仅 Cursor Cloud Agent：怎么配 Secret

官方入口：[Cloud Agents Dashboard](https://cursor.com/dashboard/cloud-agents)。排障见 [Cloud Agents 文档](https://cursor.com/docs/cloud-agent)（Secrets 不可用 / 找不到页签）。

1. 浏览器登录与跑 Cloud Agent **同一** Cursor 账号。  
2. 打开 Dashboard → **Secrets**（或当前 Environment 详情里的 **Runtime secrets**）。看不见则检查 team 权限 / 是否进错 workspace。  
3. **Add secret**：

| 字段 | 取值 |
|------|------|
| Name | **`ASCENDC_GH_PAT`**（或 `NTT_ONNX_GITHUB_TOKEN`；脚本只认这两个） |
| Value | A 节复制的 PAT |
| 类型 | 优先 **Runtime Secret**（注入为 env，对话/日志脱敏）；次选 Environment Variable |

4. 作用域尽量挂到跑 `ascendc` 的那个 **Environment**。  
5. **不要**只建名为 `GH_TOKEN` 的 secret（Cloud 常覆盖为仅对本仓有效的 `ghs_…`）。新加 secret 后须 **新开一轮** Agent。  
6. Cloud 内验证：

```bash
test -n "${ASCENDC_GH_PAT}" && echo "ASCENDC_GH_PAT is set"
FORCE=1 ONLY=ntt_onnx BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh
test -f thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h && echo OK
```

#### C. NPU 真机 / 办公室 Linux（不是 Cloud）

与 Cloud **同一私有仓**，但**不依赖 Cursor Secrets**。任选：

| 方式 | 适用 | 做法 |
|------|------|------|
| **SSH deploy key**（推荐生产机） | 长期实机 | 机器生成 ed25519 → GitHub `ntt_onnx` → Settings → **Deploy keys**（只读）→ `git clone git@github.com:bigorange1024/ntt_onnx.git thirdparty/ntt_onnx` |
| **`gh auth login`** | 人工登录的开发机 | 登录后 `bash scripts/clone-thirdparty.sh` |
| **环境变量 PAT** | CI / 无人值守 | A 节 PAT 写入实机密文（systemd EnvironmentFile / CI secret）；`export ASCENDC_GH_PAT=…` 后跑 `clone-thirdparty.sh`（**勿提交 git**） |
| **离线拷贝** | 实机无出网 | 能拉仓的机器 clone 后 `rsync`/`tar` 整棵 `thirdparty/ntt_onnx/` 到实机同路径 |

```bash
cd ~/ascendc
bash scripts/clone-thirdparty.sh
test -f thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h && echo OK
```

真机算子用 `-r npu`；与 clone 无关——缺 `ntt_onnx` 只影响依赖 LUT/golden 的用例。

#### D. 三种主机对照

| 主机 | 如何拿到 `ntt_onnx` |
|------|---------------------|
| WSL | `gh auth login` 或 SSH |
| Cursor Cloud | Dashboard Secrets → **`ASCENDC_GH_PAT`** → `clone-thirdparty.sh` |
| NPU 实机 | Deploy key / `gh auth` / 机器侧 `ASCENDC_GH_PAT` / 离线拷贝 |

### 各仓说明

| 仓 | 说明 |
|----|------|
| **tiny_sha3** | `sha3.c` / `sha3.h`；路径恒为 `thirdparty/tiny_sha3`。Host golden / 对照，**不进**默认设备生产路径。 |
| **liboqs** | 须 **0.15.0**。`clone-thirdparty.sh` **默认**调用 [`scripts/build-liboqs.sh`](../../scripts/build-liboqs.sh)（`BUILD_SHARED_LIBS=ON`、`OQS_BUILD_ONLY_LIB=ON`，并编 `liboqs_kem_ref` / `liboqs_pke_ref`）。 |
| **ascend-samples** | Gitee [ascend/samples](https://gitee.com/ascend/samples)。体积大；**勿**直接编译旧样例当 CANN 9.0 主路径。 |
| **SHA3hp** | OpenI Keccak/SHA3 AscendC；仅调研/对照。 |
| **cann-ntt** | OpenI 前向 `Ntt`；仅调研/对照。 |
| **ntt_onnx** | 原本地 `ntt_study` 工程；仓内路径一律 `thirdparty/ntt_onnx/`。CMake 工程名/目标仍可能叫 `ntt_study`（上游未改），**目录名以 ntt_onnx 为准**。 |

### liboqs build

```bash
bash scripts/build-liboqs.sh
# FORCE=1 bash scripts/build-liboqs.sh   # 强制重编
# BUILD_LIBOQS_REFS=0 …                  # 只编 lib，不编 scripts/liboqs_*_ref
```

产出：`thirdparty/liboqs/build/lib/liboqs.so`（或 `.a`）+ `scripts/liboqs_kem_ref` / `liboqs_pke_ref`。  
历史说明见 `qa/2026-06/2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md` §liboqs。

---

## 新机器 checklist

1. `git clone <本仓 URL>`
2. `bash scripts/clone-thirdparty.sh`（含 **ntt_onnx**；**默认编好 liboqs**）
3. 再按 [环境复现与开发指南.md](环境复现与开发指南.md) 配 CANN / 跑 `verify-cann.sh`
4. Cloud Agent：若缺 CANN / SIM 动态库符号异常，见 [`AGENTS.md`](../../AGENTS.md) §Cloud — **勿把 liboqs 问题与 CANN SIM 问题混为一谈**；多环境 `-r auto|verify` 见 [NPU真机环境说明.md](NPU真机环境说明.md)

---

## Agent 约束

| 禁止 | 说明 |
|------|------|
| `git add thirdparty/` | 体积大、非 ascendc 交付物；已整体 ignore |
| 删空 `thirdparty/` 子目录后不重装 | 会导致 shake/KAT/对照脚本失败 |
| `git clean -fdx` 后不恢复 | 等同删库；恢复用 `clone-thirdparty.sh` |
| 把 SHA3hp / cann-ntt **未评估** 抄进活跃探针 | 二者仅作第三方对照 |
| 再在 `thirdparty/` 下恢复 `merged_kyber/` 或 `ntt_study/` | 已迁至用例 / **ntt_onnx** |

改 URL / 钉住 tag：先改 `scripts/clone-thirdparty.sh` 的 `REPOS`，再改本文件表格。

---

## Zone.Identifier

从 Windows 拷贝可能产生 `*:Zone.Identifier`；已加入 `.gitignore`。发现时用：

```bash
find . -name '*:Zone.Identifier' -delete
find . -name '*.Identifier' -delete
```
