# 偶发上机清单 · Decrypt fused + Encrypt skel1（2026-09-03 备，明日有卡再跑）

> Decrypt 图谱：[`docs/rg-decrypt-fused.yaml`](../../docs/rg-decrypt-fused.yaml) · 试验场 [`decrypt/INDEX.md`](INDEX.md)  
> Encrypt 对照：[`NPU-TRIP-20260903-skel1.md`](../NPU-TRIP-20260903-skel1.md)  
> **原则**：先 toy 定位，再决定是否跑 stable；**干净卡**；挂死勿同卡连环重跑 Encaps/Decrypt。

## §1.1 自检（Decrypt）

| # | 项 | 状态 |
|---|----|------|
| 1 | SoftSync/GATE/骨架假设有节点；SIM 证据已写 | ✅ DGT-1..4 PASS；`J-DECRYPT-TOY-SIM-NO-HANG-SO-FAR` |
| 2 | 失败对照已沉 | ✅ TPipe-mark / TRACE 轮询污染 SIM / Encrypt toy 不能代 Decrypt |
| 3 | 上机步骤 ≤ 半页 | ✅ 见下 A→C |
| 4 | 改动可同步 | ✅ 本分支含 decrypt toys + stable `F203_DECRYPT_TRACE` |
| 5 | 失败先刷图 | ✅ 带回日志 → 主控刷新 → 再设计 |

Cloud SIM **未复现**挂死；上机目的是答 **NPU 是否挂** 与 **挂在哪段**，不是再验 toy 握手绿。

---

## 上机顺序（建议 30–45 min）

分卡：`tests=3` / `stable=1`（见 `scripts/npu_device_map.sh`）。WSL 禁 npu。

### A. Decrypt toy 骨架（卡 3，约 5 min）— 必跑

```bash
cd ascendc-tests/pass-toy-decrypt-fsm-fused-skel1
FORCE_REBUILD=1 TOY_LAUNCH_REPEAT=8 bash run.sh -r npu -v Ascend910B4
```

**期望**：不卡 `SynchronizeStream`；有 TRACE 打印；exit 0。  
**若挂**：记下第几轮、最后 TRACE、device id → **停**，同卡勿再跑 B/C。

### B. Encrypt toy skel1（同卡 3，约 5 min）— 建议

```bash
cd ascendc-tests/pass-toy-encrypt-fsm-l18-skel1
FORCE_REBUILD=1 TOY_LAUNCH_REPEAT=8 bash run.sh -r npu -v Ascend910B4
```

**期望**：见 [`NPU-TRIP-20260903-skel1.md`](../NPU-TRIP-20260903-skel1.md)；对照 Decrypt toy。

### C. stable Decrypt + TRACE（卡 1，约 10–15 min）— 核心

**仅当 A 未挂**再跑。干净卡优先。

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4
# prod input-only：默认 gen_data 已 scrub；定点 seed 便于复现
FORCE_REBUILD=1 F203_DECRYPT_TRACE=1 SEED_D=20260619 bash run.sh -r npu -v Ascend910B4
```

**期望**：

| 现象 | 含义 |
|------|------|
| PASS max=0 | NPU 上该 seed 路径不卡；`Q-ULT` 仍需更多 seed/粘性对照 |
| 卡在 SynchronizeStream | 抄全段 `[decrypt-trace] stages set=…` 与 `softSync=[s0,s1]` **末行** |
| stages 停在 0–2 附近 | prep / SoftSync0 / prep GATE |
| 停在 3–5 | NTT |
| 停在 6–8 | su_dot / SoftSync1 / su GATE |
| 停在 9–12 | INTT / extract |
| 仅 AIC 13–16 缺、AIV 已多 | AIC 槽可能假空（标量 TRACE）；以 AIV 槽+softSync 为准 |

**超时**：遵守该用例 `KERNEL_COMPUTE_BUDGET_SEC`（默认 600）；124 → 当挂死记，**勿同卡立刻再 launch**。

### D. 可选 · Encaps TRACE（卡 1，另一次会话）

仅时间富余且卡已 reset 后：

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
F203_L18_TRACE=1 bash run.sh -r npu -v Ascend910B4
```

服务 Encrypt `Q-EMPTY-TRACE` / `Q-ULT`；与 Decrypt **分开记日志**。

---

## 请带回

1. A/B/C 各自完整终端日志（含 TRACE、exit、是否 124）  
2. 卡死：挂多久、`ASCEND_DEVICE_ID`、是否 reset 后复现  
3. C 的末条 `[decrypt-trace]` + `softSync=[…]`  
4. 不必塞整包 OPPROF

## 明确不做

- 不以 toy 绿代替 stable `Q-ULT`  
- 不把 SIM 未挂写成 NPU 已消粘  
- 挂死后不连环催第二次全量 suite  
- 不改生产 FSM（除非带回 trace 后主控另开刀）
