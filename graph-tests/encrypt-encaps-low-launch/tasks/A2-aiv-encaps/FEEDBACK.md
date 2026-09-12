# FEEDBACK · A2 AIV Encaps（Host launch=1）

## 结论

**PASS**（cpu + `SIM_DIRECT=1` sim）

## Launch 审计

```text
graph-tests/aiv-kem-vector-sim/AE-P-encaps/main.cpp
  ACLRT_LAUNCH_KERNEL(aiv_encaps)  ×1
```

## 验收

| 模式 | exit | c / K vs liboqs | wall / tick | 日志 |
|------|------|-----------------|-------------|------|
| cpu | 0 | max=0 / max=0（DEVICE_FO） | ~3.4s | `aiv-ae-p-cpu.log` |
| sim | 0 | max=0 / max=0 | wall≈440s；Total tick≈1977873 | `aiv-ae-p-sim.log` |

## 门禁

- Host launch = **1**  
- 禁 `-r npu`  
- 用例根无 stray `core*.dump`
