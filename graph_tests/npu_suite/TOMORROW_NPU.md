# 明天上机（NPU 挂因取证）— 操作一页纸

> 目标：**只取证粘性挂出现在哪一档**，回报三位数字。  
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
# 确认 ccec / npu-smi 可用；默认 SOC=Ascend910B4
# graph_tests 用例默认 ASCEND_DEVICE_ID=0（可用 export 覆盖）
```

---

## 2. 一键跑整份套件（推荐）

```bash
cd graph_tests/npu_suite
bash run_all_npu.sh -v Ascend910B4
```

顺序固定：**C0 → C1 → C2**（各 1 轮，C1/C2 自动 `TOY_SKIP_GOLDEN=1`）。

| 档 | 含义 | 成功关键 Host 号 |
|----|------|----------------|
| C0 | 2-launch + SET(4) 握手壳 | `100 101 110 111` |
| C1 | Encrypt 形态粘合 | 有 `111` + L2 `402` |
| C2 | + SampleNTT Â 2×2 | 另见 `104/106`、`300–305`，最后 `111` |

单档：

```bash
cd graph_tests/npu_suite/cases/C0-e01-handshake && bash run.sh -r npu -v Ascend910B4
cd ../C1-e13-glue && bash run.sh -r npu -v Ascend910B4
cd ../C2-e15-a2x2 && bash run.sh -r npu -v Ascend910B4
```

---

## 3. 挂了怎么停

- `timeout` **124** 或墙钟 >10min 无新 TRACE → **停**，同档最多再试 **1** 次。  
- 保留各 toy 的 `output/host_trace.log`。  
- **不要**开 `OMIT_SET4` 污染首轮。

---

## 4. 你要回报什么

打开 [`REPORT_TEMPLATE.md`](REPORT_TEMPLATE.md)，对每档抄：

1. Host 三位数字序列（时间序）  
2. 设备侧三位数字（若有）  
3. **最后见到的号** / **期望下一号**  
4. PASS 或 HANG  

**禁止**贴整段长 log。

判读与下一刀推理：主控按 [`BRANCHING.md`](BRANCHING.md)。  
号段总表：[`TRACE_MASTER.md`](TRACE_MASTER.md)。  
细则：[`RUNBOOK.md`](RUNBOOK.md)。

---

## 5. 可选对照（非必须，首轮套件完成后再说）

挂因对照若要排除「KeyGen 总重编偏置」，可另开（与套件分开）：

```bash
# 示例：Encaps / KeyGen 统一 FORCE 后再压（不替代 C0–C2）
KEYGEN_FORCE_REBUILD=1 …   # 或 KEYGEN_SKIP_REBUILD=1 做对称对照
KEM_ENCAPS_FORCE_REBUILD=1 …
```

细节见 `docs/notes/Encrypt-实机无卡死-知识库.md` §2.1–2.2。
