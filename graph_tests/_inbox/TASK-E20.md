# TASK-E20 — SIM：全 Mark/CrossCore 之后的 AIV-only 尾包 stub

**deadline**：SIM 串行；禁 NPU；禁并行第二路 SIM  
**图谱**：支撑 `J-hang-after-full-trace`（收紧「末段」可活结构）  
**禁抄**：Encrypt 业务 / `f203_tail_pack_ops` / frozen

## 背景（父已钉）

真挂窗偏 **全 Mark 之后**：l18 上 AIC 在 INTT `FsmSet(PACK)` 后即结束；AIV 仍做 `mod_q` + `tail_pack_shard_gm`，**之后无 CrossCore**。  
SIM **不能**复现粘性（`J-sim-not-sticky`）；本刀只证：**该结构在 SIM 可活 + sync_audit 无真死等**，并留下可对照的 Host `1xx`。

## 实现

路径：`graph_tests/toys/toy-e20-postmark-tail/`  
骨架：从 **`toy-e17-l18-fsm-reuse13` 复制壳**（CMake/run.sh/main），**改** `mmad_custom.cpp`：

1. 保留 E17 式短序：伪 NTT `1/3` → GATE `4/8` → 伪 INTT 复用 `1/3`（或等价最短可活序）。  
2. **AIC**：INTT 最后 `FsmSet` 后 **立即 return**（模拟真 l18 AIC 早退）。  
3. **双 AIV**：在全部 CrossCore 结束后：
   - Host/设备可打「postmark」标记（槽或 Host 打印）
   - 各 AIV 用 **自建 stub**：若干轮 `TPipe` + `DataCopy` GM↔UB（假数据，**禁止** `#include` encrypt tail）
   - subBlock0 多干一点（模拟 v+两 poly），subBlock1 少干（两 poly）——非对称负载  
4. Host：`1xx` 多轮；默认 `TOY_ROUNDS=8`；`KERNEL_COMPUTE_BUDGET_SEC`≥600  

## 验收

```bash
cd graph_tests/toys/toy-e20-postmark-tail
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # ×8 须绿
# 仓库根：
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  graph_tests/toys/toy-e20-postmark-tail/mmad_custom.cpp --check all --format json \
  > /opt/cursor/artifacts/e20-sync-audit.json
```

交付：

- `STATUS.md` + `graph_tests/_outbox/FEEDBACK-E20.md`  
- 日志副本 → `/opt/cursor/artifacts/e20-default-sim.log`  
- 更新 `graph_tests/toys/INDEX.md` 一行  

## 禁

- NPU / 长 SSH / commit/push  
- 抄 `f203_encrypt_*` / `tail_pack` 实现  
- 并行 SIM；改知识库 yaml（父刷）  
