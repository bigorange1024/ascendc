# RUNBOOK — NPU 数字 TRACE 套件

> 读者：借入 NPU 实机操作者。本套件**只取证挂因**，不要求 golden/KAT。

---

## 1. 环境

```bash
cd /path/to/ascendc   # 仓库根
bash scripts/clone-thirdparty.sh   # 可选；本套件不跑 liboqs 交叉
source scripts/env.sh
```

确认 `ccec` 可用；SOC 默认 **Ascend910B4**。

---

## 2. 整份跑（推荐）

```bash
cd graph_tests/npu_suite
bash run_all_npu.sh -v Ascend910B4
```

- **顺序固定**：C0 → C1 → C2（不可并行）。  
- 每档 1 轮（`TOY_ROUNDS=1`）；C1/C2 设 `NPU_SKIP_GOLDEN=1`（仅 TRACE，不比 golden）。  
- Host TRACE 写入各 toy 的 `output/host_trace.log`（包装 tee 副本见 case 内说明）。

### 单档

```bash
cd graph_tests/npu_suite/cases/C0-e01-handshake
bash run.sh -r npu -v Ascend910B4

cd ../C1-e13-glue
bash run.sh -r npu -v Ascend910B4

cd ../C2-e15-a2x2
bash run.sh -r npu -v Ascend910B4
```

---

## 3. 超时与挂起如何停

| 档 | 默认 kernel 预算（秒） | 含义 |
|----|------------------------|------|
| C0 | 120 | 1 轮握手壳 |
| C1 | 600 | 1 轮形态粘合真链 |
| C2 | 900 | 1 轮 SampleNTT 2×2 + 粘合 |

- 预算由 `KERNEL_COMPUTE_BUDGET_SEC` 控制（各 case `run.sh` 已设默认）。  
- **`timeout` exit 124** → 记 **疑似挂死**；**不要**同档死磕重跑超过 1 次。  
- 墙钟 **>10min 无新 TRACE 行** → `Ctrl+C` 停 kernel，保留已有 `host_trace.log`。

### 实机粘性挂典型征象（对齐知识库 N1–N2）

- Host 已打印 `110`（将 launch L2）但**无** `111`（L2 Sync 回）。  
- 设备侧 **全空** 或仅有 Host `1xx`、无 `4xx/5xx`。  
- 有 `5xx` SET 前、无 SET 后 → AIV 死在 SET 前。

---

## 4. 如何只回报三位数字

1. 打开该档 `output/host_trace.log`（或 NPU 控制台 tee）。  
2. 提取**纯三位十进制行**（例 `110`），按时间序抄入 [`REPORT_TEMPLATE.md`](REPORT_TEMPLATE.md)。  
3. 标注 **最后见到的号** 与 **期望的下一号**（对照 [`TRACE_MASTER.md`](TRACE_MASTER.md)）。  
4. **禁止**贴长 log；主控只要数字序列 + 卡段。

### 快速判读

- 见 `N` 未见 `N+1`（同段语义序）→ 卡在 `N`→`N+1` 之间。  
- C0 成功：见 `100/101/110/111` 各 1 次 + 设备 `400/401/402` + `500/502` + `510/512`。  
- C1/C2 成功：Host 见 `100…111`（C2 另见 `104/106` 与 `300/302/303/304/305`）+ L2 见 `402`。

---

## 5. 本地 SIM smoke（非上机门禁）

包装验证（Cloud/WSL 无卡）：

```bash
cd graph_tests/npu_suite
bash run_smoke_sim.sh
```

每档 `SIM_DIRECT=1` 1 轮；跳过 golden。日志：`/opt/cursor/artifacts/e16-*.log`。  
C2 在 SIM 下 verify 可能因 **305 tee 偶发丢失** 非零退出；smoke 以 **kernel rc=0 + Host 111** 为准（见 `toy-e15/TRACE.md`）。

---

## 6. 故障注入（勿在实机首轮跑）

C0 缺 SET(4) 对照（**仅 SIM**；预期 124）：

```bash
cd graph_tests/toys/toy-e01-2launch-set4-trace-repeat
KERNEL_COMPUTE_BUDGET_SEC=60 TOY_ROUNDS=1 OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

实机套件 **不含** OMIT_SET4；勿用故障注入污染首轮取证。
