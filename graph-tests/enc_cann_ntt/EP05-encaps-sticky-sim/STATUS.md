# EP05-encaps-sticky-sim · STATUS

> DAG：`D-EXP-EP05`  
> 日期：2026-09-11  
> 结论：**PASS**（CPU + `SIM_DIRECT=1` sim；R=16 每轮 `c`/`K`≡liboqs Encaps **max=0**）

---

## 1. 目标

同 session sticky Encaps：EP04 形（EN13 Encrypt + Host H/G + liboqs Encaps 交叉）重复 **R≥16**；每轮换种子后 `c`/`K`≡liboqs；SIM 不挂。

## 2. 相对 EP04 / EN14

| 项 | 本刀 |
|----|------|
| 核 | **同 EN13/EP04**（未改设备核） |
| 编排 | 对齐 EN14 sticky：一次 aclInit/stream/缓冲，R 轮复用 |
| 种子 | 每轮 `input/rXX/` Encaps fixture；`EP05_ROUNDS` 默认 **16** |
| 门禁 | 每轮 `output/rXX/{c,K}.bin` ≡ liboqs Encaps |

## 3. 验收

```bash
EP05_ROUNDS=16 bash run.sh -r cpu -v Ascend910B4
EP05_ROUNDS=16 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | wall | tick | c/K max（每轮） |
|------|------|------|------|-----------------|
| CPU | 0 | **18.676s** | — | **0** ×16 |
| SIM | 0 | **2050.245s** | **12897130**（≈806k/轮） | **0** ×16 |

日志：`/opt/cursor/artifacts/enc-encaps-sim/ep05-cpu.log`、`ep05-sim.log`。  
用例根无 stray `core*.dump`；SIM 产物收拢至 `sim_log/`。禁 `-r npu`。

## 4. sync_audit

设备核相对 EN13/EP04 **未改**；红线 **0**（仅「性能」提示）。  
json：`sync_audit.json`。

## 5. 已锁参数

同 EP04；另锁 `EP05_ROUNDS` 默认 **16**；SIM 预算默认 **7200s**。  
Host 修复：`EnsureRoundOutDir` + `run.sh` 预建 `output/rXX`（防 WriteFile 失败）。
