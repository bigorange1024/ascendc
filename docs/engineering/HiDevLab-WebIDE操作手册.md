# HiDevLab WebIDE 操作手册（独立维护）

> **用途**：在 **HiDevLab（昇腾在线开发）** 上用 **WebIDE** 跑本仓 AscendC / 真机 NPU，作为 **Cursor Cloud Agent 协作真机** 的**主路径**。  
> **不**用「SSH直连」当主通道（跳板命令约 **5 分钟**失效，见 §8）。  
> **不**与 GitCode CANNLab / Tailscale 手册混用：那条线见 [`CANNLab接入与远程驱动.md`](CANNLab接入与远程驱动.md)。  
> **平台**：https://hidevlab.huawei.com/ · 用户指南 IDE 节：https://hidevlab.huawei.com/support/userGuide?currentKey=ide  

**最后刷新**：2026-09-10（首版：WebIDE 主路径 + Agent 配方协作）

---

## 0. 一句话模型

```
Cursor Cloud Agent（改代码 / 推 Git / 出配方）
        │  git push 分支
        ▼
   GitHub（bigorange1024/ascendc）
        │  WebIDE 里 git pull
        ▼
HiDevLab 云主机（A2 · CANN · 1×910B3）── WebIDE 终端执行 run.sh -r npu
        │  把日志 / 结尾 SUCCESS·FAIL 贴回聊天
        ▼
Cursor Cloud Agent（判读、改下一刀）
```

- **Agent 不直连** HiDevLab 长 SSH。  
- **人（或人在 WebIDE）** 负责：开机、进 WebIDE、粘贴 Agent 给的配方、回传日志。  
- 配方生成器：[`scripts/hidevlab/webide_recipe.sh`](../../scripts/hidevlab/webide_recipe.sh)。

---

## 1. 环境创建（一次性 / 重建时）

控制台：**在线开发 → 开发环境 → 创建环境**。

| 项 | 锁定选择 | 原因 |
|----|----------|------|
| 算力类型 | **A2** | 本仓真机主线为 910B 系；A3 规格/镜像常对不上 |
| 镜像 | **CANN 9.1.0, ubuntu** | 要 toolkit/`ccec`；ubuntu 便于 `apt`；勿选 Torch/Triton/vLLM |
| 配置规格 | **1 NPU · 算子调测**（常见 32C / 32G / 300G） | 与「算子调测」档一致 |
| 名称 | 如 `DevEnv_185447`（≤15，字母数字下划线） | 平台约束 |

创建后状态为 **运行中** 再进 WebIDE。

### 1.1 平台约束（必读）

| 约束 | 含义 | 操作 |
|------|------|------|
| 空闲 **约 1 小时**自动关机 | WebIDE / 算力停 | 长编译开着终端偶发敲键；或拆短任务 |
| 两周不启动释放 | 环境可能被清 | 重要产物及时 `git push` / 拷出 |
| 卡时配额（如 100） | 真机计费单位 | 默认只跑冒烟 + 必要刀 |
| `/workspace` | 本地高性能盘（约 300G）；**可能丢** | 工程放 `/workspace/ascendc`；以 Git 为权威 |
| `/workspace/shared_assets` 或平台说明的共享只读区 | 公共权重/包 | 只读；不要当私有仓 |

---

## 2. 进入 WebIDE

1. 环境列表 → 目标环境 **连接 → WebIDE**（官方首选）。  
2. 打开终端（`` Ctrl+` `` 或菜单 Terminal）。  
3. **可选（人肉写码）**：连接 → Cursor / VS Code，装 **`openLibing ResourceManager`**（见平台用户指南 IDE 节）。这是**本机 IDE** 路径，**不**替代本手册的 Agent 配方协作。

首次进入建议只读体检（粘贴整块）：

```bash
whoami; hostname; uname -m
cat /etc/os-release | head -5
export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${LD_LIBRARY_PATH:-}
npu-smi info | head -40
ls -l /dev/davinci* 2>/dev/null
ls -la /usr/local/Ascend
find /usr/local/Ascend -maxdepth 3 -name set_env.sh 2>/dev/null
df -h /workspace | tail -1
```

**本环境实测基线（2026-09-10，`DevEnv_185447`）**：

| 项 | 值 |
|----|-----|
| OS | Ubuntu 22.04 aarch64 |
| CANN | **9.1.0**（`/usr/local/Ascend/cann` → `cann-9.1.0`） |
| NPU | **910B3**；`npu-smi` 显示 **NPU 5**；节点 `/dev/davinci5` |
| ACL 逻辑设备 | 单卡实例仍用 **`ASCEND_DEVICE_ID=0`**（物理号≠逻辑号） |
| 编译器 | `source /usr/local/Ascend/cann/set_env.sh` 后有 `ccec` |

---

## 3. 工程落盘（WebIDE）

权威仓库：GitHub `bigorange1024/ascendc`。在 WebIDE：

```bash
mkdir -p /workspace
cd /workspace
# 首次
git clone --branch <分支名> --single-branch \
  https://github.com/bigorange1024/ascendc.git ascendc
cd ascendc
# 已有则
git fetch origin <分支名>
git checkout <分支名>
git pull --ff-only origin <分支名>
```

私有仓：用 **PAT / SSH deploy key**（只放 WebIDE 会话或用户凭据管理，**禁止**写进仓库、禁止贴进公开 PR）。

可选依赖（要 liboqs 权威交叉时）：

```bash
cd /workspace/ascendc
bash scripts/clone-thirdparty.sh   # 较慢；仅冒烟可先跳过
```

---

## 4. Agent ↔ WebIDE 协作协议（强制）

| 角色 | 做 | 不做 |
|------|----|------|
| **Cloud Agent** | 在约定分支改代码、`commit`/`push`；用 `webide_recipe.sh` 生成**可粘贴**配方；根据回传日志定下一刀 | 依赖「SSH直连」长会话；把跳板 token / 连接密码写入 Git |
| **人（WebIDE）** | 开机 → WebIDE → `git pull` → 粘贴配方 → 把**完整尾日志**贴回聊天 | 改 Agent 未指定的分支参数绕过失败 |

### 4.1 一刀标准格式（Agent 输出）

```text
【HiDevLab WebIDE 配方】
分支: <branch>
目录: /workspace/ascendc/<相对路径>
粘贴:

<source cann + LD_LIBRARY_PATH + ASCEND_DEVICE_ID=0 + run.sh ...>

期望尾部关键字: [SUCCESS] 或 verify PASS
回传: 从编译结束到结尾的完整终端输出
```

### 4.2 一刀标准动作（人）

1. 确认环境 **运行中**，进 WebIDE。  
2. `cd /workspace/ascendc && git pull --ff-only`。  
3. 整块粘贴配方，等待结束。  
4. 复制输出贴回 Agent（可截断编译刷屏，**保留错误与 SUCCESS/FAIL 段**）。

---

## 5. 真机运行约定

```bash
source /usr/local/Ascend/cann/set_env.sh
export LD_LIBRARY_PATH=/usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64/driver:/usr/local/Ascend/driver/lib64/common:${LD_LIBRARY_PATH:-}
export ASCEND_DEVICE_ID=0
export CANNLAB=1          # 借用仓内「单卡默认逻辑 0」开关；名虽 cannlab，语义适用
export CMAKE_BUILD_JOBS="${CMAKE_BUILD_JOBS:-8}"
```

| 项 | 值 |
|----|-----|
| SoC 参数 `-v` | **`Ascend910B3`**（以 `npu-smi` Name 为准；变了先改手册再跑） |
| 模式 | `bash run.sh -r npu -v Ascend910B3` |
| 设备号 | **必须**显式 `ASCEND_DEVICE_ID=0`；勿把物理 **5** 传给 `aclrtSetDevice` |

### 5.1 推荐冒烟（建环境 / 换机后第一刀）

```bash
cd /workspace/ascendc/ascendc-tests/add_custom
ASCEND_DEVICE_ID=0 CANNLAB=1 CMAKE_BUILD_JOBS=8 \
  bash run.sh -r npu -v Ascend910B3
```

期望：`[SUCCESS] output matches golden`（或同类 SUCCESS）。

### 5.2 长任务

- 拆成可中断刀；或 `nohup ... > /workspace/ascendc/_hidevlab_run.log 2>&1 &` 后 `tail -f`。  
- 防 **1h 空闲关机**：终端保持活动或分段跑。  
- 结束看 `_hidevlab_run.log`，把尾部贴回 Agent。

---

## 6. 配方脚本（Agent / 人本地均可跑）

仓库内：

```bash
# 在 Cursor Agent 工作区或已 clone 的机器上
bash scripts/hidevlab/webide_recipe.sh
bash scripts/hidevlab/webide_recipe.sh add_custom
bash scripts/hidevlab/webide_recipe.sh kem-keygen
bash scripts/hidevlab/webide_recipe.sh --branch cursor/hidevlab-cloud-npu-9099 --case add_custom
```

脚本**只打印**应粘贴到 WebIDE 的命令，不发起 SSH。

---

## 7. 与其它路径的边界

| 路径 | 何时用 | 文档 |
|------|--------|------|
| **本手册 · WebIDE** | HiDevLab + Cloud Agent 协作真机（**默认**） | 本文 |
| openLibing + 本机 Cursor/VS Code | 人在本机 IDE 改 HiDevLab 上的文件 | 平台用户指南 IDE 节 |
| SSH直连 | 仅救急（§8） | 本文 §8 |
| GitCode CANNLab + Tailscale | 旧真机线；**另一套环境** | [`CANNLab接入与远程驱动.md`](CANNLab接入与远程驱动.md) |

**禁止**：把 CANNLab 的 `cannlab-npu` / 旧 IP 写进本手册当 HiDevLab 默认；两套并行时以**当前任务指定的环境**为准。

---

## 8. SSH直连（非主路径 · 仅说明）

平台「连接 → SSH直连」会给出：

```text
ssh -J jt_<id>:<短时token>@<跳板>:2234 root@<容器IP>
连接密码: <密码>
```

| 事实 | 处置 |
|------|------|
| 跳板命令约 **5 分钟**有效 | 不当 Agent 主通道 |
| 超期 → `platform authorization denied` | 重新点「SSH直连」 |
| 连接密码寿命通常长于跳板 token | 仍勿入库 |

Agent **默认不**维护长期 `~/.ssh/hidevlab_*` 依赖；若临时用过，密钥/密码只放本机、不进 Git。

---

## 9. 故障速查

| 现象 | 优先检查 |
|------|----------|
| `npu-smi: libc_sec.so` | `LD_LIBRARY_PATH` 是否含 driver `lib64{,/driver,/common}` |
| `ccec: command not found` | 是否 `source /usr/local/Ascend/cann/set_env.sh` |
| `aclrtSetDevice` 107001 | 是否误用物理卡号；改回 `ASCEND_DEVICE_ID=0` |
| `git pull` 鉴权失败 | PAT/权限；或改为公开可读分支 |
| 环境变「已关机」 | 空闲 1h；控制台再启动后 WebIDE 重进，`git status` 看 `/workspace` 是否还在 |
| 卡时耗尽 | 控制台配额；停非必要实例 |

---

## 10. 维护规则

1. **本文独立维护**：HiDevLab 行为变更（配额、空闲、镜像名、设备号）只改本文 + `scripts/hidevlab/`，**不要**塞进 `CANNLab接入与远程驱动.md`。  
2. 改默认分支 / 冒烟用例 / `-v` SoC 时，同步改 §2 基线表与 `webide_recipe.sh` 默认值。  
3. 登记：[`docs/engineering/INDEX.md`](INDEX.md)、根 [`AGENTS.md`](../../AGENTS.md) 入口链。  
4. 当日决策写入 `qa/YYYY-MM/YYYY-MM-DD-….md`。

---

## 11. 检查清单（每次上机）

- [ ] 环境运行中；WebIDE 能开终端  
- [ ] `/workspace/ascendc` 在目标分支且 `git pull` 最新  
- [ ] `source cann` + `LD_LIBRARY_PATH` + `ASCEND_DEVICE_ID=0`  
- [ ] `-v Ascend910B3` 与 `npu-smi` 一致  
- [ ] 配方跑完；日志尾部已回传 Agent  
- [ ] 长任务已考虑 1h 空闲关机  
- [ ] **未**把 SSH 跳板 token / 密码提交进 Git  
