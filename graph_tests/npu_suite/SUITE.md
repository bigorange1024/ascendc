# NPU 数字 TRACE 套件（Encrypt 挂因取证）

**目的**：服务 `Q-root-cause`（实机粘性挂可操作根因）。**不是**正确性/KAT 门禁。  
**图谱节点**：`D-exp-e16` · `D-trace-digits` · `D-defer-correctness-until-hang`  
**知识库**：[`docs/notes/Encrypt-实机无卡死-知识库.md`](../../docs/notes/Encrypt-实机无卡死-知识库.md) §6

---

## 套件组成

| 档 | 目录 | 来源 toy | 挂因阶梯 |
|----|------|----------|----------|
| **C0** | [`cases/C0-e01-handshake/`](cases/C0-e01-handshake/) | `toy-e01-2launch-set4-trace-repeat` | 2-launch + SET(4) 握手壳（8→1 轮上机） |
| **C1** | [`cases/C1-e13-glue/`](cases/C1-e13-glue/) | `toy-e13-encrypt-shaped-glue` | Encrypt 形态粘合（L1 采样 / L2 代数+压码 → c1∥c2） |
| **C2** | [`cases/C2-e15-a2x2/`](cases/C2-e15-a2x2/) | `toy-e15-samplentt-a-full-2x2` | C1 + 完整 2×2 Â SampleNTT（独立 launch phase） |

**薄包装**：各 case 仅 `run.sh` + `ORIGIN.md`，调用原 toy 的 `run.sh`；**不改** E01–E15 源目录。

---

## 文档索引

| 文件 | 用途 |
|------|------|
| [`TOMORROW_NPU.md`](TOMORROW_NPU.md) | **明天上机一页纸（先读这个）** |
| [`RUNBOOK.md`](RUNBOOK.md) | NPU 按序跑、超时停、只报三位数字 |
| [`TRACE_MASTER.md`](TRACE_MASTER.md) | 套件级 TRACE 总表（号段 + 判读） |
| [`BRANCHING.md`](BRANCHING.md) | **主控锁**：测什么 × 反馈分支 → 下一刀推理 |
| [`REPORT_TEMPLATE.md`](REPORT_TEMPLATE.md) | 主控对照用打字格式（用户勿填勿回传） |
| [`run_all_npu.sh`](run_all_npu.sh) | 实机整份串行入口（C0→C1→C2） |
| [`run_rxn_npu.sh`](run_rxn_npu.sh) | **B3 下一刀**：默认 C2×7 多轮粘性 |
| [`run_smoke_sim.sh`](run_smoke_sim.sh) | 本地 SIM smoke（包装未坏；跳过 golden） |

---

## 明确不做（用户锁 2026-09-06）

- ByteDecode / t̂
- liboqs / KAT / 权威交叉
- 复测 retracted 充分条件路线
- 改 Encrypt / 原 toy 源码

---

## 上机前置

1. CANN 9.0 + 借入机分卡策略见 [`docs/engineering/NPU真机环境说明.md`](../../docs/engineering/NPU真机环境说明.md)。  
2. `graph_tests/` 路径默认 **ASCEND_DEVICE_ID=0**（`npu_device_map.sh` → other）。  
3. 每档独立编译；**禁止并行**多路 SIM/NPU kernel。  
4. 回报：用户**只在对话打字**发三位编码（例 `C1 HANG 110`）；禁止回传任何文件。
