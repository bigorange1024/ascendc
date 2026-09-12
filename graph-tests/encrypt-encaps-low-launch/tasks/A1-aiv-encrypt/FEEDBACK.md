# FEEDBACK · A1 AIV Encrypt（Host launch=1 · 仅 SIM 战役）

> 日期：2026-09-12  
> 用例：`graph-tests/aiv-kem-vector-sim/AE-E-encrypt`  
> 结论：**PASS**（cpu + `SIM_DIRECT=1` sim；未跑 npu）

## Launch 审计

```bash
rg -n '^\s*ACLRT_LAUNCH_KERNEL' main.cpp
# 174:    ACLRT_LAUNCH_KERNEL(aiv_encrypt)
# call_count=1
```

| 项 | 结果 |
|----|------|
| Host `ACLRT_LAUNCH_KERNEL` | **1**（`main.cpp:174`） |
| CPU 路径 | `ICPU_RUN_KF` ×1（非 ACLRT launch） |
| 证据 | `/opt/cursor/artifacts/low-launch-sim/aiv-ae-e-launch-audit.txt` |

## 验收命令（顺序执行）

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | max diff | wall / tick | 日志 |
|------|------|----------|-------------|------|
| cpu | **0** | **c max=0**（mism_bytes=0） | wall≈2.6–3.0s | `/opt/cursor/artifacts/low-launch-sim/aiv-ae-e-cpu.log` |
| sim | **0** | **c max=0**（mism_bytes=0） | wall≈420s；**Total tick = 1885464** | `/opt/cursor/artifacts/low-launch-sim/aiv-ae-e-sim.log` |

说明：`aiv-ae-e-sim.log` 内另有一次后续重跑亦 PASS（tick≈1885589，wall≈294s）；本战役主证据取首轮 tick=1885464。

## 门禁核对

| 门禁 | 结果 |
|------|------|
| Host launch = 1 | **达成** |
| 禁 `-r npu` | **遵守** |
| 禁并行多路 SIM | **遵守**（先完整 AE-E 再 AE-P） |
| 用例根 stray dump（`core*.dump` / `OPPROF_*`） | **无**（见 `aiv-ae-e-stray-check.txt`） |
| 代码改动 | **无**（launch 已=1，未改实现） |

## 阻塞点

无。
