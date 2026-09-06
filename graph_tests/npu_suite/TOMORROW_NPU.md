# 明天上机（NPU 挂因取证）— 操作一页纸

> 目标：只取证粘性挂出现在哪一档。  
> **反馈信道（硬约束）**：你**不能回传任何文件**；最多在对话里**打字**回报三位错误/TRACE 编码。  
> 主控**禁止**再要求你交 log、模板 md、截图文件包。  
> **不做**：ByteDecode、KAT、正确性比对、并行多档。

---

## 0. 拉代码

```bash
git fetch origin
git checkout cursor/kem-2launch-sticky-1534
git pull --ff-only origin cursor/kem-2launch-sticky-1534
```

---

## 1. 环境

```bash
cd /path/to/repo
source scripts/env.sh
# 默认 SOC=Ascend910B4；graph_tests 默认卡 0（可 export ASCEND_DEVICE_ID 覆盖）
```

---

## 2. 跑

```bash
cd graph_tests/npu_suite
bash run_all_npu.sh -v Ascend910B4
```

顺序：**C0 → C1 → C2**（各 1 轮）。某档挂死可停更高档，但打字时写明「未跑」。

| 档 | 含义 | 成功时你应看到的关键号 |
|----|------|----------------------|
| C0 | 握手壳 | Host `100 101 110 111` |
| C1 | 形态粘合 | 有 `111`（以及尽量有 `402`） |
| C2 | +Â 2×2 | 另有 `104/106`、`300…305`，最后 `111` |

挂了：`timeout` 124 或 >10min 无新数字 → 停；同档最多再试 1 次。

---

## 3. 你只需要在对话里打字（整段复制改数字）

看屏幕上打印的**三位数字**（或最后几个），按下面格式打字发我即可：

```text
C0: 100 101 110 111 | PASS
C1: 100 101 102 105 110 | HANG last=110 expect=111
C2: SKIP
```

或更短：

```text
C0 PASS
C1 HANG 110
C2 SKIP
```

| 可打 | 含义 |
|------|------|
| `PASS` | 该档跑完且见到 `111` |
| `HANG last=NNN` | 卡住；NNN=你最后亲眼看到的三位号 |
| `TIMEOUT` | 124 / 久无输出 |
| `SKIP` | 没跑（例如前档已挂） |
| 可选 `dev=400 401 …` | 若屏幕上还有设备侧三位号，顺手打上；没有就省略 |

**不要**：传文件、贴整屏 log、贴 md 附件。  
主控收到打字后按 `BRANCHING.md` 推理下一刀。
