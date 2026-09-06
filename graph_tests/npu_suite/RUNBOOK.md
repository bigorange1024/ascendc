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

## 4. 如何回报（只能打字，不能回传文件）

**硬约束**：操作者无法回传 log/文件；最多在对话里打字发三位编码。  
格式见 [`TOMORROW_NPU.md`](TOMORROW_NPU.md) §3 / [`REPORT_TEMPLATE.md`](REPORT_TEMPLATE.md)。

示例：

```text
C0 PASS
C1 HANG 110
C2 SKIP
```

### 快速判读（主控）

- 见 `N` 未见下一关键号 → 卡在该边界。  
- C0 成功：`100/101/110/111`。  
- C1/C2 成功：有 `111`（C2 尽量确认 SampleNTT 段）。

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
