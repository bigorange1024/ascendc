# 实机上机 — 测什么 / 怎么测 / 怎么回报

> **目标**：只取证「粘性挂出现在哪一档」，不是验收正确性。  
> **反馈**：你**不能回传任何文件**；最多在对话里**打字**发 `REPORT:` / `SUMMARY` 行。  
> **不做**：ByteDecode、KAT、golden 比对、并行多档。

---

## 测什么（三档阶梯）

| 档 | 内容 | 挂因判定（只看 Host） |
|----|------|----------------------|
| **C0** | 2-launch + SET(4) 空壳 | 有 `REPORT: C0 PASS` → 壳不挂 |
| **C1** | Encrypt 形态粘合（无 Â） | 有 `REPORT: C1 PASS` → 真链不挂 |
| **C2** | C1 + Â 2×2 SampleNTT | 有 `REPORT: C2 PASS` → 含 Â 不挂 |

**成功**：Host 见到 `111`（套件打印 `REPORT: Cx PASS last=111`）。  
**粘性挂**：timeout 124，或 `REPORT: Cx HANG last=110`（有 110 无 111）。

注意：屏幕上的 `NTT Test: N=...` 只是 gen_data 脚本噪音，**不是**档位名。  
设备侧 `200/400/...` 在实机 **经常打印不出来**（`AscendC::printf`）；旧版因此误报 `L1 TRACE 200 missing`——**那不是挂**，已改成 NPU 只验 Host。

---

## 怎么测

```bash
git fetch origin
git checkout cursor/kem-2launch-sticky-1534
git pull --ff-only origin cursor/kem-2launch-sticky-1534

cd /path/to/ascendc
source scripts/env.sh
cd graph_tests/npu_suite
bash run_all_npu.sh -v Ascend910B4
```

顺序 C0→C1→C2；某档挂也会继续后面档。盯住：

- `##### NPU_SUITE CASE C0/C1/C2 START #####`
- 结尾的 `REPORT: ...`
- 最后的 `SUMMARY C0:OK` 等

---

## 怎么回报（只打字）

把最后看到的 `REPORT:` / `SUMMARY` 原样打字发回，例如：

```text
REPORT: C0 PASS last=111
REPORT: C1 PASS last=111
REPORT: C2 HANG last=110
SUMMARY C0:OK
SUMMARY C1:OK
SUMMARY C2:TIMEOUT124
```

或更短：

```text
C0 PASS
C1 PASS
C2 HANG 110
```

**不要**传文件 / 整屏 log。
