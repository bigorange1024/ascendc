# 实机上机 — 测什么 / 怎么测 / 怎么回报

> **目标**：只取证「粘性挂出现在哪一档」，不是验收正确性。  
> **反馈**：你**不能回传任何文件**；最多在对话里**打字**发三位 TRACE 编码。  
> **不做**：ByteDecode、KAT、golden 比对、并行多档、故障注入。

---

## 测什么（三档阶梯，找挂点）

| 档 | 测的是什么 | 绿了说明什么 | 挂了说明什么 |
|----|------------|--------------|--------------|
| **C0** | 2-launch + SET(4) **空壳握手**（无真链） | 实机握手/旗语壳能活 | 壳就挂 → 同步/launch 问题，别怪业务核 |
| **C1** | Encrypt **形态粘合**（采样→NTT→basemul→INTT→Compress→ByteEncode，无 Â SampleNTT） | 真链在无 SampleNTT 时能跑完 | 挂在业务链 / L2 Wait(4) 一带（历史主挂点） |
| **C2** | C1 + **Â 完整 2×2 SampleNTT**（独立 launch） | 含 Â 的组装在实机也能跑完 | 挂在 SampleNTT 段或与粘合交界 |

**成功关键点（三档共用）**：屏幕上见到 Host **`111`**（L2 Sync 回）。  
**历史主挂征象**：见到 **`110`**（将 launch L2）但**没有** `111`。

本轮**不测**：正确性、ByteDecode、KAT、生产 Encaps/Encrypt 原路径。

---

## 怎么测（命令）

### 0. 拉代码

```bash
git fetch origin
git checkout cursor/kem-2launch-sticky-1534
git pull --ff-only origin cursor/kem-2launch-sticky-1534
```

### 1. 环境

```bash
cd /path/to/ascendc          # 换成你的仓库根
source scripts/env.sh
# 默认 Ascend910B4；换卡：export ASCEND_DEVICE_ID=<卡号>
```

### 2. 一键串行（推荐）

```bash
cd graph_tests/npu_suite
bash run_all_npu.sh -v Ascend910B4
```

- 顺序固定：**C0 → C1 → C2**，**禁止并行**。  
- 每档只跑 **1 轮**；已跳过 golden。  
- 某档挂死可 `Ctrl+C` 停，**更高档可跳过**，打字时写 `SKIP`。

### 3. 何时算挂、何时停

| 现象 | 动作 |
|------|------|
| `timeout` exit **124** | 记 **HANG/TIMEOUT**；同档最多再试 **1** 次 |
| **>10 分钟**屏幕无新三位数字 | `Ctrl+C` 停；记最后看到的号 |
| 正常结束且见到 **`111`** | 该档 **PASS** |

单档（可选）：

```bash
cd graph_tests/npu_suite/cases/C0-e01-handshake && bash run.sh -r npu -v Ascend910B4
cd ../C1-e13-glue && bash run.sh -r npu -v Ascend910B4
cd ../C2-e15-a2x2 && bash run.sh -r npu -v Ascend910B4
```

更细号表：同目录 [`TRACE_MASTER.md`](TRACE_MASTER.md)。分支决策：[`BRANCHING.md`](BRANCHING.md)（你不用填，主控用）。

---

## 怎么回报（只打字）

看屏幕上的**三位数字**，在对话里发：

```text
C0 PASS
C1 HANG 110
C2 SKIP
```

或稍详：

```text
C0: 100 101 110 111 | PASS
C1: 100 101 102 105 110 | HANG last=110 expect=111
C2: SKIP
```

| 可打 | 含义 |
|------|------|
| `PASS` | 该档结束且见到 `111` |
| `HANG last=NNN` | 卡住；NNN=你最后亲眼看到的号 |
| `TIMEOUT` | 124 / 久无输出 |
| `SKIP` | 没跑 |
| 可选 `dev=400 401 …` | 若还看到设备侧号，顺手打上；没有就省略 |

**不要**：传文件、贴整屏 log、贴 md。
