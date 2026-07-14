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
| `ntt_onnx/` | 外部 clone | https://github.com/bigorange1024/ntt_onnx.git | 默认分支（浅） | NTT/LUT golden（原 `ntt_study`） | **`clone-thirdparty.sh`** |

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

若 `ntt_onnx` 为**私有仓**且 HTTPS 404：

```bash
gh repo clone bigorange1024/ntt_onnx thirdparty/ntt_onnx
# 或：git clone git@github.com:bigorange1024/ntt_onnx.git thirdparty/ntt_onnx
```

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
